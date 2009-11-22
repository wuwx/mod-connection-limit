mod_connection_limit.la: mod_connection_limit.slo
	$(SH_LINK) -rpath $(libexecdir) -module -avoid-version  mod_connection_limit.lo
DISTCLEAN_TARGETS = modules.mk
shared =  mod_connection_limit.la
