package = io-agent
version = 1.0.0
tarname = $(package)
distdir = $(tarname)-$(version)

all clean tio-agent:
	cd src && $(MAKE) $@ AGENT_VERSION=$(version)

dist: $(distdir).tar.gz

$(distdir).tar.gz: $(distdir)
	tar chof - $(distdir) | gzip -9 -c > $@
	rm -rf $(distdir)

$(distdir): FORCE
	mkdir -p $(distdir)/src
	cp Makefile $(distdir)
	cp src/Makefile $(distdir)/src
	cp src/die_with_message.c $(distdir)/src
	cp src/read_line.c $(distdir)/src
	cp src/read_line.h $(distdir)/src
	cp src/logmsg.c $(distdir)/src
	cp src/io_socket.c $(distdir)/src
	cp src/io_serial.c $(distdir)/src
	cp src/io_agent.c $(distdir)/src
	cp src/io_agent.h $(distdir)/src
        
FORCE:
	-rm $(distdir).tar.gz > /dev/null 2>&1
	-rm -rf $(distdir) > /dev/null 2>&1
        
.PHONY: FORCE all clean dist
