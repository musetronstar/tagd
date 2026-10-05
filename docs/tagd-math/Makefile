# Dependencies (Debian/Mint/Ubuntu):
#	sudo apt install make latexmk texlive-latex-recommended texlive-latex-extra lmodern
# Uses TeX Live's pdfLaTeX, similar to Overleaf; no shell escape required.

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

LATEXMK ?= latexmk
LATEXMK_FLAGS ?= -pdf -interaction=nonstopmode -halt-on-error -file-line-error -synctex=1
DOCUMENT := tagd-math

.PHONY: all pdf clean distclean watch help
all: pdf
pdf:
	$(LATEXMK) $(LATEXMK_FLAGS) $(DOCUMENT).tex

# latexmk tracks dependencies and repeats LaTeX until references and TOC settle.
watch:
	$(LATEXMK) $(LATEXMK_FLAGS) -pvc $(DOCUMENT).tex

# Keep the PDF; distclean removes it as well.
clean:
	$(LATEXMK) -c $(DOCUMENT).tex
	rm -f $(DOCUMENT).synctex.gz

distclean: clean
	$(LATEXMK) -C $(DOCUMENT).tex

help:
	@echo 'make            Build tagd-math.pdf (automatic dependency tracking)'
	@echo 'make watch      Rebuild when source changes'
	@echo 'make clean      Remove auxiliary files; keep PDF'
	@echo 'make distclean  Remove auxiliary files and PDF'
