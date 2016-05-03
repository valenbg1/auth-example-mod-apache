mod_auth_example.la: mod_auth_example.slo
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version  mod_auth_example.lo
DISTCLEAN_TARGETS = modules.mk
shared =  mod_auth_example.la
