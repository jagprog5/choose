.PHONY: no install uninstall

no:
	echo "instead run 'make install' or 'make uninstall'"

install:
	scripts/install.sh

uninstall:
	scripts/uninstall.sh
