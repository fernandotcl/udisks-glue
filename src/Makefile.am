bin_PROGRAMS = udisks-glue

udisks_glue_SOURCES = \
    dbus_constants.h \
    filter.c \
    filter.h \
    filters.c \
    filters.h \
    globals.h \
    handlers.c \
    handlers.h \
    main.c \
    match.c \
    match.h \
    matches.c \
    matches.h \
    props.c \
    props.h \
    property_cache.c \
    property_cache.h \
    session.c \
    session.h \
    tracked_object.c \
    tracked_object.h \
    util.c \
    util.h

udisks_glue_CPPFLAGS = \
    -std=c99 -D_GNU_SOURCE -Wall \
    -DSYSCONFDIR=\"$(sysconfdir)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_GLIB_CFLAGS) \
    $(LIBCONFUSE_CFLAGS)

udisks_glue_LDADD = \
    $(GLIB_LIBS) \
    $(DBUS_GLIB_LIBS) \
    $(LIBCONFUSE_LIBS)
