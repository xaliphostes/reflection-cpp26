MARP := npx -y @marp-team/marp-cli@latest
SRC  := presentation.md
GEN  := .presentation-light.md

.PHONY: all dark light clean

all: dark light

# Dark deck: built straight from the canonical source (has `class: invert`).
dark: $(SRC)
	$(MARP) $(SRC) --pdf -o presentation-dark.pdf

# Light deck: same content, with every `invert` directive stripped out.
#   - drops the global `class: invert` front-matter line
#   - turns `<!-- _class: invert lead -->` into `<!-- _class: lead -->`
light: $(SRC)
	sed -e '/^class: invert$$/d' \
	    -e 's/_class: invert /_class: /g' \
	    -e 's|href="light.html"|href="index.html"|' \
	    -e 's|🔆 Light version|🌙 Dark version|' \
	    $(SRC) > $(GEN)
	$(MARP) $(GEN) --pdf -o presentation-light.pdf
	rm -f $(GEN)

clean:
	rm -f presentation-dark.pdf presentation-light.pdf $(GEN)
