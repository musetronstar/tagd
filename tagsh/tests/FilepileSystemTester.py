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
        return self.run_cli('--tagspace', 'test', '--tagl', '>>+ ' + self.url + ';', *args)

    def test_store_option_applies_predicates_to_each_source(self):
        second = self.home / 'holiday photo.jpg'
        second.write_bytes(b'holiday')
        third = self.home / 'punctuation;"'
        third.write_bytes(b'punctuation')
        result = subprocess.run(
            [BIN, '--home', str(self.home), '--tagspace', 'test', '--noshell',
             '--tagl', '>> tags _sub _entity; >> picture _sub _entity;',
             '--store', self.source.name, str(second), 'file://' + str(third),
             '--with', '_type_of picture _has tags = "beach, friends"',
             '--tagl', '<< ' + self.url + '; << ' + second.as_uri() +
             '; << ' + third.as_uri() + ';'],
            cwd=self.home, text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(sum(line.startswith('link:hd:') for line in result.stdout.splitlines()), 3)
        self.assertEqual(result.stdout.count('tags = "beach, friends"'), 3)
        self.assertEqual(result.stdout.count('_type_of picture'), 3)

    def test_store_groups_follow_action_order_and_duplicate_rules(self):
        second = self.home / 'second'
        second.write_bytes(b'second')
        self.run_cli('--tagspace', 'other', '--create').check_returncode()
        result = self.run_cli(
            '--tagspace', 'test', '--store', self.url,
            '--with', '_has _message = first',
            '--tagl', '%% _use other;', '--store', str(second),
            '--with', '_has _message = second')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(result.stdout.splitlines()), 2)
        changed = self.run_cli('--tagspace', 'test', '--store', self.url,
                               '--with', '_has _message = changed')
        self.assertEqual(changed.returncode, 0, changed.stderr)
        self.assertIn('#001', changed.stdout)
        duplicate = self.run_cli('--tagspace', 'test', '--store', self.url, str(second),
                                 '--with', '_has _message = changed')
        self.assertNotEqual(duplicate.returncode, 0)
        self.assertIn('ts_duplicate', duplicate.stderr)
        self.assertEqual(duplicate.stdout, '')
        ignored = self.run_cli('--tagspace', 'test', '--tagl',
                               '%% _ignore_duplicates _true;', '--store', self.url,
                               '--with', '_has _message = changed')
        self.assertEqual(ignored.returncode, 0, ignored.stderr)
        self.assertEqual(changed.stdout, ignored.stdout)
        dumped = self.run_cli('--tagspace', 'test', '--dump')
        self.assertNotIn(second.as_uri(), dumped.stdout)
        other = self.run_cli('--tagspace', 'other', '--tagl', '<< ' + second.as_uri() + ';')
        self.assertIn('_message = second', other.stdout)

    def test_store_option_rejects_invalid_argument_groups(self):
        for args in [('--store',), ('--store', '--noshell'), ('--store', ''),
                     ('--with', '_has _message = x'),
                     ('--store', self.url, '--with'),
                     ('--store', self.url, '--with', '  '),
                     ('--store', self.url, '--tagl', '<< _entity;', '--with', '_has _message = x'),
                     ('--store', self.url, '--with', '_has _message = x', '--with', '_has _message = y'),
                     ('--store', self.url, '--bad-option')]:
            with self.subTest(args=args):
                result = self.run_cli('--tagspace', 'test', *args)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((self.home / 'filepile').exists())

    def test_store_predicate_fragment_cannot_execute_commands(self):
        for predicates in ['_has _message = ok; >> unwanted _sub _entity;',
                           '_has _message = ok\n\n>> unwanted _sub _entity;',
                           '%% _ignore_duplicates _true;', '_has missing_object = x']:
            with self.subTest(predicates=predicates):
                result = self.run_cli('--tagspace', 'test', '--store', self.url,
                                      '--with', predicates)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((self.home / 'filepile').exists())
                dumped = self.run_cli('--tagspace', 'test', '--dump')
                self.assertNotIn('unwanted', dumped.stdout)
                self.assertNotIn(self.url, dumped.stdout)
        valid = self.run_cli('--tagspace', 'test', '--store', self.url,
                             '--with', '_has _message = "beach; friends >> fun"',
                             '--tagl', '<< ' + self.url + ';')
        self.assertEqual(valid.returncode, 0, valid.stderr)
        self.assertIn('beach; friends >> fun', valid.stdout)

    def test_store_group_stops_after_first_runtime_failure(self):
        later = self.home / 'later'
        later.write_bytes(b'later')
        result = self.run_cli('--tagspace', 'test', '--store', str(self.source),
                              str(self.home / 'missing'), str(later),
                              '--tagl', '>> forbidden _sub _entity;')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('ts_not_found', result.stderr)
        self.assertEqual(len(result.stdout.splitlines()), 1)
        dumped = self.run_cli('--tagspace', 'test', '--dump')
        self.assertIn(self.url, dumped.stdout)
        self.assertNotIn(later.as_uri(), dumped.stdout)
        self.assertNotIn('forbidden', dumped.stdout)

    def test_store_accepts_shell_globs_and_preserves_source_aliases(self):
        photos = self.home / 'photos'
        photos.mkdir()
        (photos / 'holiday photo.jpg').write_bytes(b'holiday')
        (photos / '-alias.jpg').symlink_to(self.source)
        result = subprocess.run(
            ['bash', '-c', 'exec "$1" --home "$2" --tagspace test --noshell '
             "--store ./photos/*.jpg --with '_has _message = \"glob\"'",
             'store-test', BIN, str(self.home)], cwd=self.home,
            text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(result.stdout.splitlines()), 2)
        alias = self.run_cli('--tagspace', 'test', '--tagl',
                             '<< ' + (photos / '-alias.jpg').as_uri() + ';')
        self.assertEqual(alias.returncode, 0, alias.stderr)
        self.assertIn('_message = glob', alias.stdout)

    def test_store_path_parent_components_follow_symlinks(self):
        left, right = self.home / 'left', self.home / 'right'
        left.mkdir()
        (right / 'nested').mkdir(parents=True)
        (left / 'jump').symlink_to(right / 'nested', target_is_directory=True)
        (left / 'chosen').write_bytes(b'wrong')
        expected = right / 'chosen'
        expected.write_bytes(b'correct')
        result = self.run_cli('--tagspace', 'test', '--store', str(left / 'jump/../chosen'))
        self.assertEqual(result.returncode, 0, result.stderr)
        content = list((self.home / 'filepile').glob('*/*/*'))
        self.assertEqual(len(content), 1)
        self.assertEqual(content[0].read_bytes(), b'correct')
        declared = self.run_cli('--tagspace', 'test', '--tagl', '<< ' + expected.as_uri() + ';')
        self.assertEqual(declared.returncode, 0, declared.stderr)

    def test_store_option_keeps_http_acquisition_deferred(self):
        result = self.run_cli('--tagspace', 'test', '--store', 'https://example.com/photo.jpg')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('ts_not_implemented', result.stderr)
        self.assertFalse((self.home / 'filepile').exists())

    def test_warning_and_duplicates(self):
        self.source.write_bytes(b'')
        first = self.run_cli('--tagspace', 'test', '--store', self.url)
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

    def test_dump_and_protected_reload_with_source_encoding(self):
        first = self.store('--dump')
        self.assertEqual(first.returncode, 0, first.stderr)
        link, dump = first.stdout.split('\n', 1)
        self.assertIn('#001', link)
        self.assertIn('_file:stored', dump)
        self.assertIn('_stored_time = "', dump)
        source = self.home / 'saved.tagl'
        source.write_text(dump)
        loaded = self.run_cli('--file', str(source), '--dump')
        self.assertNotEqual(loaded.returncode, 0)
        self.assertIn('ts_misuse', loaded.stderr)
        self.assertIn('_type_of _file:stored', dump)
        self.assertNotIn('_is_a _file:stored', dump)
        repeated = self.store()
        self.assertNotEqual(repeated.returncode, 0)
        self.assertIn('ts_duplicate', repeated.stderr)

    def test_direct_database_and_volatile_targets(self):
        db = self.home / 'tagspaces/test/test.db'
        result = self.run_cli('--db', str(db), '--tagl', '>>+ ' + self.url + ';', home=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('--db disables home directory lookup', result.stderr)
        for target in ('_config', '_default'):
            result = self.run_cli('--tagspace', target, '--tagl', '>>+ ' + self.url + ';')
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('active tagspace is volatile', result.stderr)
        self.assertFalse((self.home / 'filepile').exists())

    def test_script_origins_and_nested_include(self):
        statement = '%% _ignore_duplicates _true; >>+ ' + self.url + ';\n'
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
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('link:hd:', result.stdout)
        redirected = subprocess.run([BIN, '--home', str(self.home), '--tagspace', 'test'],
                                    input=statement, text=True, capture_output=True, timeout=10)
        self.assertEqual(redirected.returncode, 0, redirected.stderr)
        self.assertIn('link:hd:', redirected.stdout)

    def test_redirected_input_stops_on_command_and_tagl_errors(self):
        for invalid in ('.nosuchcmd', '.load /nonexistent/filepile-input',
                        '>> broken _sub missing_parent;'):
            result = subprocess.run([BIN, '--home', str(self.home), '--tagspace', 'test'],
                                    input=invalid + '\n>> must_not_exist _sub _entity;\n',
                                    text=True, capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            dumped = self.run_cli('--tagspace', 'test', '--dump')
            self.assertNotIn('must_not_exist', dumped.stdout)

    def test_filepile_write_refusal_is_reported_once(self):
        valid = 'filepile:///aa/aa/' + 'a' * 64 + '.bin'
        for subject in (valid, '"filepile:///bad"', '"FILEPILE:///x"', 'filepile:x',
                        '"hd:!!!/bad!!!!!!filepile"'):
            with self.subTest(subject=subject):
                result = self.run_cli('--tagspace', 'test', '--tagl',
                                      '>> ' + subject + ' _has _message = forbidden;')
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stderr.count('_error:tagl_err'), 1, result.stderr)
                self.assertNotIn('forbidden', self.run_cli('--tagspace', 'test', '--dump').stdout)

    def test_never_stored_filepile_delete_reports_not_found(self):
        missing = 'filepile:///aa/aa/' + 'a' * 64 + '.bin'
        result = self.run_cli('--tagspace', 'test', '--tagl', '!!+ ' + missing + ';')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('_error:ts_not_found', result.stderr)
        self.assertIn(missing, result.stderr)
        self.assertNotIn('system-owned', result.stderr)

    def test_stored_url_delete_removes_final_sources_but_keeps_original_files(self):
        self.store().check_returncode()
        content = next((self.home / 'filepile').glob('*/*/*'))
        stored = 'filepile:///' + content.relative_to(self.home / 'filepile').as_posix()
        result = self.run_cli('--tagspace', 'test', '--tagl', '!!+ ' + stored + ';', '--dump')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn(self.url, result.stdout)
        self.assertNotIn(stored, result.stdout)
        self.assertNotIn('link:hd:', result.stdout)
        self.assertFalse(content.exists())
        self.assertTrue(self.source.exists())

    def test_managed_delete_all_local_input_modes(self):
        statement = '!!+ ' + self.url + ';\n'
        script = self.home / 'delete.tagl'
        script.write_text(statement)
        parent = self.home / 'delete-parent.tagl'
        parent.write_text('%% _include "delete.tagl";')
        for args, stdin in [
            (('--tagl', statement), None),
            (('--tagl', '-'), statement),
            (('--file', str(script)), None),
            (('--file', str(parent)), None),
        ]:
            self.store().check_returncode()
            result = self.run_cli('--tagspace', 'test', *args, input=stdin)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(self.source.exists())
            self.assertNotIn('link:hd:', self.run_cli('--tagspace', 'test', '--dump').stdout)
        self.store().check_returncode()
        redirected = subprocess.run([BIN, '--home', str(self.home), '--tagspace', 'test'],
                                    input=statement, text=True, capture_output=True, timeout=10)
        self.assertEqual(redirected.returncode, 0, redirected.stderr)

    def test_managed_delete_grammar_and_missing_warning(self):
        declared = self.run_cli('--tagspace', 'test', '--tagl',
                                '>>+ ' + self.url + ' _has _message = "user";')
        self.assertEqual(declared.returncode, 0, declared.stderr)
        predicate = self.run_cli('--tagspace', 'test', '--tagl',
                                '!!+ ' + self.url + ' _has _message = "user";')
        self.assertEqual(predicate.returncode, 0, predicate.stderr)
        content = list((self.home / 'filepile').glob('*/*/*'))
        self.assertEqual(len(content), 1)
        self.assertTrue(content[0].is_file())
        ordinary = self.run_cli('--tagspace', 'test', '--tagl', '!! ' + self.url + ';')
        self.assertNotEqual(ordinary.returncode, 0)
        self.assertIn('ts_dependency', ordinary.stderr)
        invalid = self.run_cli('--tagspace', 'test', '--tagl',
                              '!!+ ' + self.url + ' _sub _file;')
        self.assertNotEqual(invalid.returncode, 0)
        self.assertTrue(content[0].exists())
        content[0].unlink()
        result = self.run_cli('--tagspace', 'test', '--tagl', '!!+ ' + self.url + ';')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('warning:', result.stderr)
        self.assertIn('ts_not_found', result.stderr)
        self.assertIn('already missing', result.stderr)
        self.assertTrue(self.source.exists())

    def test_store_types_and_delete_type_and_modifier_grammar(self):
        result = self.run_cli('--tagspace', 'test', '--tagl',
                              '>> picture _sub _entity; >>+ ' + self.url +
                              ' _type_of picture _has _message = "caption";')
        self.assertEqual(result.returncode, 0, result.stderr)
        for suffix in ('_type_of picture', '_has _message = "caption"', '_has _message'):
            result = self.run_cli('--tagspace', 'test', '--tagl',
                                  '!!+ ' + self.url + ' ' + suffix + ';', '--dump')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('link:hd:', result.stdout)
            self.assertTrue(list((self.home / 'filepile').glob('*/*/*')))
        self.assertNotIn('_type_of picture', result.stdout)
        self.assertNotIn('_message', result.stdout)

    def test_delete_diagnostics_with_logging_disabled(self):
        invalid = self.run_cli('--tagspace', 'test', '--log-level', 'emergency',
                               '--tagl', '>> dog _sub _entity; !!+ dog;')
        self.assertNotEqual(invalid.returncode, 0)
        self.assertIn('whole-subject managed DELETE requires a URL', invalid.stderr)
        self.store().check_returncode()
        content = next((self.home / 'filepile').glob('*/*/*'))
        content.unlink()
        content.symlink_to(self.source)
        failed = self.run_cli('--tagspace', 'test', '--log-level', 'emergency',
                              '--tagl', '!!+ ' + self.url + ';')
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn('deletion committed; physical cleanup incomplete', failed.stderr)
        self.assertTrue(self.source.exists())

    def test_managed_delete_normalizes_store_source_spelling(self):
        alternate = self.url.replace('file:///', 'file://localhost/')
        stored = self.run_cli('--tagspace', 'test', '--tagl', '>>+ ' + alternate + ';')
        self.assertEqual(stored.returncode, 0, stored.stderr)
        deleted = self.run_cli('--tagspace', 'test', '--tagl', '!!+ ' + alternate + ';')
        self.assertEqual(deleted.returncode, 0, deleted.stderr)
        self.assertFalse(list((self.home / 'filepile').glob('*/*/*')))

    def test_interactive_store_and_delete(self):
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
            os.write(master, ('>>+ ' + self.url + ';\n').encode())
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
            os.write(master, ('!!+ ' + self.url + ';\n.exit\n').encode())
            child.wait(timeout=5)
            self.assertTrue(self.source.exists())
            self.assertNotIn('link:hd:', self.run_cli('--tagspace', 'test', '--dump').stdout)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
            os.close(master)


if __name__ == '__main__':
    unittest.main()
