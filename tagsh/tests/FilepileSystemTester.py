#!/usr/bin/env python3
"""Filepile command contracts against the built tagsh executable."""
import errno
import os
from pathlib import Path
import pty
import select
import subprocess
import tempfile
import time
import unittest

BIN = str((Path(__file__).resolve().parent / '../bin/tagsh').resolve())


class FilepileSystemTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tagsh-filepile-')
        self.home = Path(self.temp.name)
        self.source = self.home / 'source ! # % é'
        self.source.write_bytes(b'filepile system test\n')
        self.url = self.source.as_uri()
        self.run_cli('--tagspace', 'test', '--create').check_returncode()

    def tearDown(self):
        self.temp.cleanup()

    def run_cli(self, *args, input=None, home=True):
        prefix = ['--home', str(self.home)] if home else []
        return subprocess.run([BIN, *prefix, '--noshell', *args], input=input,
                              text=True, capture_output=True, timeout=10)

    def store(self, *args):
        return self.run_cli('--tagspace', 'test', '--store', self.url, *args)

    def test_warning_and_duplicates(self):
        self.source.write_bytes(b'')
        first = self.store()
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertIn('warning: source file is empty (0 bytes)', first.stderr)
        self.assertNotIn('warning:', first.stdout)
        self.assertTrue(first.stdout.startswith('link:hd:'))
        second = self.store('--tagl', '>> forbidden _sub _entity;')
        self.assertNotEqual(second.returncode, 0)
        self.assertIn('ts_duplicate', second.stderr)
        self.assertEqual(second.stdout, '')
        ignored = self.run_cli('--tagspace', 'test', '--tagl',
                               '%% _ignore_duplicates _true;', '--store', self.url)
        self.assertEqual(ignored.returncode, 0, ignored.stderr)
        self.assertEqual(first.stdout, ignored.stdout)

    def test_dump_reload_and_source_encoding(self):
        first = self.store('--dump')
        self.assertEqual(first.returncode, 0, first.stderr)
        link, dump = first.stdout.split('\n', 1)
        self.assertIn('#001', link)
        self.assertIn('_file:stored', dump)
        self.assertIn('_stored_time = "', dump)
        source = self.home / 'saved.tagl'
        source.write_text(dump)
        loaded = self.run_cli('--file', str(source), '--dump')
        self.assertEqual(loaded.returncode, 0, loaded.stderr)
        self.assertEqual(loaded.stdout, dump)
        repeated = self.store()
        self.assertNotEqual(repeated.returncode, 0)
        self.assertIn('ts_duplicate', repeated.stderr)

    def test_direct_database_and_volatile_targets(self):
        db = self.home / 'tagspaces/test/test.db'
        result = self.run_cli('--db', str(db), '--store', self.url, home=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('--db disables home directory lookup', result.stderr)
        for target in ('_config', '_default'):
            result = self.run_cli('--tagspace', target, '--store', self.url)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('active tagspace is volatile', result.stderr)
        self.assertFalse((self.home / 'filepile').exists())

    def test_script_origins_and_nested_include(self):
        statement = '.store ' + self.url + '\n'
        script = self.home / 'script.tagl'
        script.write_text(statement)
        parent = self.home / 'parent.tagl'
        parent.write_text('%% _include "script.tagl";')
        for args, stdin in [
            (('--tagl', statement), None),
            (('--tagl', '-'), statement),
            (('--file', str(script)), None),
            (('--file', str(parent)), None),
        ]:
            result = self.run_cli('--tagspace', 'test', *args, input=stdin)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertFalse((self.home / 'filepile').exists())
        redirected = subprocess.run([BIN, '--home', str(self.home), '--tagspace', 'test'],
                                    input=statement, text=True, capture_output=True, timeout=10)
        self.assertNotEqual(redirected.returncode, 0)
        self.assertIn('requires interactive terminal input', redirected.stderr)
        self.assertFalse((self.home / 'filepile').exists())

    def test_redirected_input_stops_on_command_and_tagl_errors(self):
        for invalid in ('.nosuchcmd', '.load /nonexistent/filepile-input',
                        '>> broken _sub missing_parent;'):
            result = subprocess.run([BIN, '--home', str(self.home), '--tagspace', 'test'],
                                    input=invalid + '\n>> must_not_exist _sub _entity;\n',
                                    text=True, capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            dumped = self.run_cli('--tagspace', 'test', '--dump')
            self.assertNotIn('must_not_exist', dumped.stdout)

    def test_interactive_store_uses_terminal_origin(self):
        master, slave = pty.openpty()
        child = subprocess.Popen([BIN, '--home', str(self.home), '--tagspace', 'test'],
                                 stdin=slave, stdout=slave, stderr=slave)
        os.close(slave)
        output = b''
        try:
            deadline = time.monotonic() + 10
            while b'tagd> ' not in output and time.monotonic() < deadline:
                if select.select([master], [], [], 0.2)[0]:
                    try:
                        output += os.read(master, 65536)
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
                        break
            self.assertIn(b'tagd> ', output)
            os.write(master, ('.store ' + self.url + '\n').encode())
            output = b''
            while b'#001' not in output and time.monotonic() < deadline:
                if select.select([master], [], [], 0.2)[0]:
                    try:
                        output += os.read(master, 65536)
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
                        break
            self.assertIn(b'link:hd:', output)
            self.assertIn(b'#001', output)
            os.write(master, b'.exit\n')
            child.wait(timeout=5)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
            os.close(master)


if __name__ == '__main__':
    unittest.main()
