# Folders
SOURCES  := src
TARGET   := target
BUILDDIR := $(TARGET)/build

# Compiler stuff
LIBS     := gstreamer-0.10
CPPFLAGS := -Isrc -g -Wall -Werror $(shell pkg-config --cflags $(LIBS))
LDFLAGS  := -lid3 $(shell pkg-config --libs $(LIBS))

# Project's stuff
PLUGIN   := id3v23mux
PROJECT  := gst-$(PLUGIN)-tags
SVN_REPO := $(shell svn info | grep -E '^URL: ' | cut -f2 -d' ' | sed -e 's%/trunk%%')
VERSION  := $(shell head -n1 CHANGELOG.txt)
DIST     := $(PROJECT)-$(VERSION)


SAMPLE := samples/a.mp3


.PHONY: plugin
plugin: $(BUILDDIR) $(BUILDDIR)/libgst$(PLUGIN).so


.PHONY: info
info:
	@echo "PROJECT:  $(PROJECT)"
	@echo "PLUGIN:   $(PLUGIN)"
	@echo "VERSION:  $(VERSION)"
	@echo "DIST:     $(DIST)"
	@echo "SVN_REPO: $(SVN_REPO)"


$(BUILDDIR)/libgst$(PLUGIN).so: $(BUILDDIR)/gst$(PLUGIN).o $(BUILDDIR)/gsttaglibmux.o
	g++ -shared $(LDFLAGS) -o $@ $(BUILDDIR)/gst$(PLUGIN).o $(BUILDDIR)/gsttaglibmux.o


$(BUILDDIR)/gst$(PLUGIN).o: $(SOURCES)/gst$(PLUGIN).cc $(SOURCES)/gst$(PLUGIN).h $(SOURCES)/gsttaglibmux.c $(SOURCES)/gsttaglibmux.h src/config.h
	g++ -DHAVE_CONFIG_H -fPIC -c $(CPPFLAGS) -o $@ $<


$(BUILDDIR)/gsttaglibmux.o: $(SOURCES)/gsttaglibmux.c $(SOURCES)/gsttaglibmux.h src/config.h
	g++ -DHAVE_CONFIG_H -fPIC -c $(CPPFLAGS) -o $@ $<


.PHONY: test
test: plugin
	rm -f ~/.gstreamer-0.10/registry.* || true
	gst-launch --gst-debug=$(PLUGIN):5 --gst-plugin-path=$(BUILDDIR) filesrc location=$(SAMPLE) ! id3demux ! $(PLUGIN) ! fakesink


.PHONY: test-write
test-write: $(TARGET) plugin
	rm -f ~/.gstreamer-0.10/registry.* || true
	gst-launch --gst-debug=$(PLUGIN):5 --gst-plugin-path=$(BUILDDIR) filesrc location=$(SAMPLE) ! id3demux ! $(PLUGIN) ! filesink location=$(TARGET)/copy.mp3


.PHONY: test-leaks
test-leaks: $(TARGET) plugin
	rm -f ~/.gstreamer-0.10/registry.* || true
	valgrind --leak-check=full --trace-children=yes \
	  gst-launch --gst-debug=$(PLUGIN):5 --gst-plugin-path=$(BUILDDIR) \
	    filesrc location=$(SAMPLE) ! id3demux ! $(PLUGIN) ! filesink location=$(TARGET)/copy.mp3 2> valgrind.txt


.PHONY: install
install: plugin
	mkdir -p ~/.gstreamer-0.10/plugins/
	cp $(BUILDDIR)/libgst$(PLUGIN).so ~/.gstreamer-0.10/plugins/
	rm  ~/.gstreamer-0.10/registry.* || true


.PHONY: inspect
inspect: install
	gst-inspect $(PLUGIN)


.PHONY: tag
tag:
	-svn rm -m "Retagging version $(VERSION)" $(SVN_REPO)/tags/$(VERSION)
	svn cp -m "Tag for version $(VERSION)" $(SVN_REPO)/trunk $(SVN_REPO)/tags/$(VERSION)


.PHONY: dist
dist: $(TARGET)
	tar zcf $(TARGET)/$(DIST).tar.gz \
	  CHANGELOG.txt Makefile README.txt TODO.txt src \
	  --exclude=.svn --exclude=$(DIST).tar.gz --transform 's,^,$(DIST)/,'


src/config.h: CHANGELOG.txt Makefile
	@echo '#define GST_PACKAGE_NAME   "GStreamer Pending Plug-ins"' > $@
	@echo '#define GST_PACKAGE_ORIGIN "GStreamer Bugzilla"' >> $@
	@echo '#define PLUGIN             "$(PLUGIN)"' >> $@
	@echo '#define PACKAGE            "$(PROJECT)"' >> $@
	@echo '#define VERSION            "$(VERSION)"' >> $@


.PHONY: clean
clean:
	rm -rf $(TARGET) > /dev/null 2>&1 || true


$(TARGET):
	mkdir -p $(TARGET) 2> /dev/null || true


$(BUILDDIR):
	mkdir -p $(BUILDDIR) 2> /dev/null || true
