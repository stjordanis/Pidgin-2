if PLUGINS
	subdir('kwallet')

	if nettle.found()
		internalkeyring_plugin = library('internalkeyring', 'internalkeyring.c',
		    dependencies : [nettle, libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if secretservice.found()
		secretsservice_plugin = library('secretservice', 'secretservice.c',
		    dependencies : [secretservice, libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif

	if IS_WIN32
		wincred_plugin = library('wincred', 'wincred.c',
		    dependencies : [libpurple_dep],
		    name_prefix : '',
		    install : true, install_dir : PURPLE_PLUGINDIR)
	endif
endif  # PLUGINS
