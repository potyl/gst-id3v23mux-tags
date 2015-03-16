This project provides a gstreamer plugin that encodes media files (mainly MP3 audio files) with ID3 tags version 2.3.0 metadata. The ID3 tags are used to store meta information about the media file. This information includes elements such as the track's title, author, performing artist, etc.

The plugin is named 'id3v23mux' and can be used as any other gstreamer tagger.

The gstreamer framework provides already some ID3 taggers, at least two of them. One that encodes the in version 1.0 and the other in 2.4. Today most software and hardware mp3 players are able to understand at least one of the two formats. Although not all MP3 players (particularly the hardware ones) are able to understand ID3 v2.4 tags and using ID3 v1.0 can have some limitations specially with tags that contain UNICODE characters. It's most likely that today a decent MP3 player will be able to understand ID3 v2.3 tags. This plugin was written in order to provide an alternative to MP3 players that can understand only ID3 v2.3 tags.

The ID3 v2.3 encoding is actually performed through the library id3lib which is available at http://www.id3lib.org/. The plugin it self depends only on the gstreamer framework (version 0.10) and the library id3lib (version 3.8.3).

This plugin was submitted for inclusion into Gstreamer but no reaction has given by the project maintainers. If you find this plugin useful leave a comment in the following Gstreamer Bugzilla entry: http://bugzilla.gnome.org/show_bug.cgi?id=459226.