kdj_modules=__init__ constants finder tools muxer demuxer ui_main ui_editor ui_options

install-exec-local:
	${mkinstalldirs} "$(DESTDIR)$(pythondir)/kdj"
	for module in $(kdj_modules); do \
	  $(INSTALL_DATA) "${top_srcdir}/tools/KateDJ/kdj/$$module.py" "$(DESTDIR)$(pythondir)/kdj"; \
	  ${top_srcdir}/misc/autotools/py-compile --destdir="$(DESTDIR)" --basedir="${prefix}" "${DESTDIR}$(pythondir)/kdj/$$module.py"; \
	done
	${mkinstalldirs} "$(DESTDIR)$(bindir)"
	${INSTALL_SCRIPT} "${top_srcdir}/tools/KateDJ/KateDJ" "$(DESTDIR)$(bindir)"

uninstall-local:
	for module in $(kdj_modules); do \
	  rm "$(DESTDIR)$(pythondir)/kdj/$$module.py"; \
	  rm "$(DESTDIR)$(pythondir)/kdj/$$module.pyc"; \
	  rm "$(DESTDIR)$(pythondir)/kdj/$$module.pyo"; \
	done
	rmdir "$(DESTDIR)$(pythondir)/kdj"
	rm "$(DESTDIR)$(bindir)/KateDJ"

EXTRA_DIST=KateDJ $(foreach module, $(kdj_modules), kdj/$(module).py)
