/* GStreamer id3lib-based ID3v23 (2.3) muxer
 * Copyright 2008 - Emmauel Rodriguez <emmanuel.rodriguez@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-id3v23mux
 * @see_also: #GstID3Demux, #GstTagSetter
 *
 * <refsect2>
 * <title>ID3 tags in the version 2.3</title>
 * <para>
 * This plugin encodes ID3 tags in the version 2.3. The ID3 tags consist
 * of a special region in an MP3 used to store meta information about the
 * MP3. This information contains elements such a tile, artist and so on.
 * </para>
 *
 * <para>
 * The ID3 tags can be written in multiple versions: 1.0, 2.3 and 2.4. Each 
 * format has it advantages and inconveniences. For instance version 1.0 is
 * very portable but has no support Unicode. On the other hand version 2.4 is
 * the most advanced tagging format but is not supported by all MP3 hardware
 * players; the reason why this plugin exists is because the Sansa e250 MP3
 * player can't handle 2.4 tags. The version 2.3 while being less advanced than
 * its successor (2.4) is handled by more MP3 players and has supports Unicode.
 * </para>
 *
 * <para>
 * The tagging is performed through the C++ library id3lib and relies on a copy
 * of the good/ext/taglib sub-framework available in GStreamer.
 *
 * This plugin is a simple tagger. Here's a sample example on how to retag
 * an existing MP3:
 * <programlisting>
 * gst-launch filesrc location=old.mp3 ! id3demux ! id3v23mux ! filesink location=new.mp3
 * </programlisting>
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstid3v23mux.h"

#include <string.h>

#include <id3/tag.h>
#include <gst/tag/tag.h>


#define TAG_ADD_FRAME(tag, frame) if (frame != NULL) {tag.AddFrame(frame);}


GST_DEBUG_CATEGORY_STATIC (gst_id3v23_mux_debug);
#define GST_CAT_DEFAULT gst_id3v23_mux_debug

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS("application/x-id3")
);


GST_BOILERPLATE(GstId3v23Mux, gst_id3v23_mux, GstTagLibMuxPriv, GST_TYPE_TAG_LIB_MUX);


static GstBuffer* gst_id3v23_mux_render_tag (
	GstTagLibMuxPriv *mux,
	GstTagList   *taglist
);

static void gst_id3v23_mux_base_init (gpointer g_class) {
	GstElementClass *element_class = GST_ELEMENT_CLASS(g_class);
	gst_element_class_add_pad_template(
		element_class,
		gst_static_pad_template_get(&src_template)
	);

	// Old versions of GStreamer are missing gst_element_class_set_details_simple()
	GstElementDetails details = GST_ELEMENT_DETAILS(
		// The API wants gchar* but these are static strings (const gchar*).
		g_strdup("id3lib ID3v2.3 Muxer"),
		g_strdup("Formatter/Metadata"),
		g_strdup("Adds an ID3v2.3 header to the beginning of MP3 files using id3lib"),
		g_strdup("Emmanuel Rodriguez <emmanuel.rodriguez@gmail.com>")
	);
	gst_element_class_set_details(element_class, &details);
	g_free(details.longname);
	g_free(details.klass);
	g_free(details.description);
	g_free(details.author);


	GST_DEBUG_CATEGORY_INIT(
		gst_id3v23_mux_debug,
		PLUGIN, 
		0, 
		"id3lib-based ID3v2.3 tag muxer"
	);
}

static void gst_id3v23_mux_class_init (GstId3v23MuxClass *klass) {
	GST_TAG_LIB_MUX_CLASS(klass)->render_tag = GST_DEBUG_FUNCPTR(gst_id3v23_mux_render_tag);
}

static void gst_id3v23_mux_init (GstId3v23Mux *id3v23mux, GstId3v23MuxClass *id3v23mux_class) {
	// nothing to do
}





// Custom methods and functions
static void tags_print_loop (
	const GstTagList *tags, 
	const gchar      *tag, 
	const gpointer   user_data
);

static ID3_Frame* tags_tag_to_frame (
	const GstTagList  *tags,
	const gchar       *tag,
	const ID3_FrameID id
);

static ID3_Frame* tags_text_to_frame (
	const gchar       *value,
	const ID3_FrameID id
);

static ID3_Frame* tags_composed_tags_to_frame (
	const GstTagList  *tags,
	const gchar       *left,
	const gchar       *right,
	const ID3_FrameID id
);

static ID3_Frame* tags_image_tag_to_frame (
	const GstTagList  *tags,
	const gchar       *tag,
	const ID3_FrameID id
);

static gchar* tags_tag_to_string (
	const GstTagList *tags, 
	const gchar      *tag
);

static GDate* tags_tag_to_date(
	const GstTagList *tags, 
	const gchar      *tag
);

static void tags_set_frame_field_to_unicode (
	const ID3_Frame   *frame,
	const ID3_FieldID field_id,
	const gchar       *value
);

static size_t tags_utils_number_length (
	const guint i
);

static unicode_t* tags_utils_utf8_to_utf16 (
	const gchar *text
);

static gboolean tags_buffer_has_data (
	const GstBuffer *buffer
);










// ------------------
//  Instance methods
// ------------------


//
// Prints a tag from the given tag list.
//
// This function is meant to be used by gst_tag_list_foreach().
//
static void tags_print_loop (
	const GstTagList *tags, 
	const gchar      *tag, 
	gpointer         user_data
) {
	
	gchar *value = tags_tag_to_string(tags, tag);
	g_free(value);
	
	return;
}


//
// This function writes the gstreamer tags that have been collected so far.
//
static GstBuffer* gst_id3v23_mux_render_tag (
	GstTagLibMuxPriv * mux, 
	GstTagList *tags
) {
	

	// Print the tags (DEBUG)
	gst_tag_list_foreach(tags, tags_print_loop, NULL);
	
	// Trivial frames (tag -> frame)
	ID3_Frame *title = tags_tag_to_frame(tags, GST_TAG_TITLE, ID3FID_TITLE);	
	ID3_Frame *album = tags_tag_to_frame(tags, GST_TAG_ALBUM, ID3FID_ALBUM);	
	ID3_Frame *artist = tags_tag_to_frame(tags, GST_TAG_ARTIST, ID3FID_LEADARTIST);	
	ID3_Frame *genre = tags_tag_to_frame(tags, GST_TAG_GENRE, ID3FID_CONTENTTYPE);
	
	// Composed frames (two gst tags -> 1 frame)
	ID3_Frame *track_number = tags_composed_tags_to_frame(tags, GST_TAG_TRACK_NUMBER, GST_TAG_TRACK_COUNT, ID3FID_TRACKNUM);
	ID3_Frame *part_in_set = tags_composed_tags_to_frame(tags, GST_TAG_ALBUM_VOLUME_NUMBER, GST_TAG_ALBUM_VOLUME_COUNT, ID3FID_PARTINSET);


	GDate *track_date = tags_tag_to_date(tags, GST_TAG_DATE);
	ID3_Frame *frame_year = NULL;
	ID3_Frame *frame_date = NULL;
	if (track_date != NULL) {
		
		// The year frame format YYYY
		GDateYear year = g_date_get_year(track_date);
		if (year != G_DATE_BAD_YEAR) {
			gchar *value = g_strdup_printf("%04u", year);
			frame_year = tags_text_to_frame(value, ID3FID_YEAR);
			g_free(value);
		}
	
		// The date frame format DDMM
		GDateMonth month = g_date_get_month(track_date);
		GDateDay day = g_date_get_day(track_date);
		if (month != G_DATE_BAD_MONTH && day != G_DATE_BAD_DAY) {
			gchar *value = g_strdup_printf("%02u%02u", day, month);
			frame_date = tags_text_to_frame(value, ID3FID_DATE);
			g_free(value);
		}
	}
	g_free(track_date);
	
	
	// Images
	ID3_Frame *image = tags_image_tag_to_frame(tags, GST_TAG_IMAGE, ID3FID_PICTURE);
	ID3_Frame *image_preview = tags_image_tag_to_frame(tags, GST_TAG_PREVIEW_IMAGE, ID3FID_PICTURE);
	

	// Add the frames to the tag
	ID3_Tag tag;
	TAG_ADD_FRAME(tag, title);
	TAG_ADD_FRAME(tag, artist);
	TAG_ADD_FRAME(tag, album);
	TAG_ADD_FRAME(tag, part_in_set);
	TAG_ADD_FRAME(tag, track_number);
	TAG_ADD_FRAME(tag, genre);
	TAG_ADD_FRAME(tag, frame_year);
	TAG_ADD_FRAME(tag, frame_date);
	TAG_ADD_FRAME(tag, image);
	TAG_ADD_FRAME(tag, image_preview);
	
	// Write the tag's binary data into a gstreamer buffer
	int length = tag.Size() * (sizeof(unicode_t)/sizeof(uchar));
	uchar *data = new uchar[length];
	tag.Render(data, ID3TT_ID3V2);
	GstBuffer *buffer = gst_buffer_new_and_alloc(length);
	memcpy(GST_BUFFER_DATA(buffer), data, length);
	gst_buffer_set_caps(buffer, GST_PAD_CAPS(mux->srcpad));
	delete [] data;

	delete title;
	delete artist;
	delete album;
	delete part_in_set;
	delete track_number;
	delete genre;
	delete frame_year;
	delete frame_date;
	delete image;
	delete image_preview;
	
	return buffer;
}


//
// Returns a frame who's value is composed of two numeric tags.
// Ideally this function is used to return frames in the fashion:
//   "01/10"  (Ideal for track numbers and parts in sets).
//
// If the right tag is missing then the left tag will be returned
// alone in a string.
//
// The frame will have the numbers padded with 0 at the beginning.
// The number of zeros to use depends on the number of digits used
// by the longest number.
//
// Parameters:
//   tagger: the plugin used to collect the tags.
//   left:   the left tag to lookup.
//   right:  the right tag to lookup.
//
// Returns:
//   The value of the tag as a string or NULL if the tag couldn't be found.
//
//
static ID3_Frame* tags_composed_tags_to_frame(
	const GstTagList  *tags, 
	const gchar       *left,
	const gchar       *right,
	const ID3_FrameID id
) {
	
	// The values to render
	gboolean found;
	guint left_value;
	found = gst_tag_list_get_uint(tags, left, &left_value);
	if (! found) {
		// Assume that his is the first item of composed group (set or track) 
		left_value = 1;
	}
	
	// Check if the right value is there
	guint right_value = left_value;
	found = gst_tag_list_get_uint(tags, right, &right_value);
	if (! found) {
		// Return a single value
		gchar *as_string = g_strdup_printf("%u", left_value);
		ID3_Frame *frame = tags_text_to_frame(as_string, id);
		g_free(as_string);
		
		return frame;
	}
	
	
	// Compute the format, it depends on the longest number (number of digits)
	size_t left_length = tags_utils_number_length(left_value);
	size_t right_length = tags_utils_number_length(right_value);
	size_t length = left_length < right_length ? right_length : left_length;
	gchar *format = g_strdup_printf("%%0%uu/%%0%uu", length, length);

	// The text to add to the content of the tag
	gchar *composed = g_strdup_printf(format, left_value, right_value);
	g_free(format);


	ID3_Frame *frame = tags_text_to_frame(composed, id);
	g_free(composed);

	return frame;
}


//
// Returns the length (number of digits) of the given number.
//
static size_t tags_utils_number_length(const guint i) {
	// Compute the length by counting the number
	// of characters in the string representation
	gchar *string = g_strdup_printf("%u", i);
	size_t length = strlen(string);
	g_free(string);
	return length;
}


//
// Returns the value of the given tag as an UTF-8 string.
// If the tag holds a value that's not a string, the value
// will be converted to a string.
//
//
// Parameters:
//   tags: the tags collected so far.
//   tag:  the tag to lookup.
//
// Returns:
//   The value of the tag as a string or NULL if the tag couldn't be found.
//   The value has to be freed with g_free.
//
static gchar* tags_tag_to_string(
	const GstTagList *tags, 
	const gchar      *tag
) {

	gchar *value = NULL;
	gboolean copied = FALSE;

	GType type = gst_tag_get_type(tag);
	switch (type) {
	
		case G_TYPE_STRING:
		{
			copied = gst_tag_list_get_string(tags, tag, &value);
			if (copied) {
				GST_LOG("Tag %s has a STRING of value %s", tag, value);
			}
		}
		break;
		
		case G_TYPE_UINT:
		{
			guint tmp;
			copied = gst_tag_list_get_uint(tags, tag, &tmp);
			if (copied) {
				value = g_strdup_printf("%u", tmp);
				GST_LOG("Tag %s has an UINT of value %s", tag, value);
			}
		}
		break;
		
		case G_TYPE_UINT64:
		{
			guint64 tmp;
			gst_tag_list_get_uint64(tags, tag, &tmp);
			if (copied) {
				value = g_strdup_printf("%" G_GUINT64_FORMAT, tmp);
				GST_LOG("Tag %s has an UINT64 of value %s", tag, value);
			}
		}
		break;
		
		default:
		{
		
			// Some types are not defined through constants but through functions!
			// In such a case it's impossible to use a switch/case
			if (type == GST_TYPE_DATE || type == G_TYPE_DATE) {
				GDate *tmp = NULL;
				copied = gst_tag_list_get_date(tags, tag, &tmp);
				if (copied) {
					GDateYear year = g_date_get_year(tmp);
					if (copied) {
						value = g_strdup_printf("%u", year);
						GST_LOG("Tag %s has a DATE with year %s", tag, value);
					}
				}
				g_free(tmp);
				break;
			}

			GST_WARNING("Tag %s is a non supported type (%s)", tag, g_type_name(type));
			value = g_strdup("");
			copied = TRUE;
		}
	}


	if (! copied) {
		value = NULL;
	}
	
	return value;
}


//
// Returns the value of the given tag as a date.
//
//
// Parameters:
//   tags: the tags collected so far.
//   tag:  the tag to lookup.
//
// Returns:
//   The value of the tag as a date or NULL if the tag can't be found.
//   The return value has to be freed with g_free.
//
static GDate* tags_tag_to_date(
	const GstTagList *tags,
	const gchar      *tag
) {
	GDate *tmp = NULL;
	gboolean copied = gst_tag_list_get_date(tags, tag, &tmp);
	if (! copied) {return NULL;}
	return tmp;
}


//
// Converts a GST image tag into an ID3 tag.
//
//
// Parameters:
//   tags: the tags collected so far.
//   tag:  the tag to lookup.
//
// Returns:
//   The value of the tag as an image or NULL if the tag can't be found.
// 
//
static ID3_Frame* tags_image_tag_to_frame (
	const GstTagList  *tags, 
	const gchar       *tag,
	const ID3_FrameID id
) {
	
	guint size = gst_tag_list_get_tag_size(tags, tag);
	if (size > 1) {
		GST_WARNING("Tag %s has more than one value (%d), but only one tag will be written", tag, size);
	}
	
	// Get the data of the image (if there's an image)
	const GValue *value = gst_tag_list_get_value_index(tags, tag, 0);
	if (value == NULL) {return NULL;}
	
	GstBuffer *image = (GstBuffer *) gst_value_get_mini_object(value);
	if (! tags_buffer_has_data(image)) {
		GST_WARNING("Image buffer has no data");
		return NULL;
	}

	
	GstStructure *structure = gst_caps_get_structure(GST_BUFFER_CAPS(image), 0);
	const gchar *mime_type = gst_structure_get_name(structure);

	const gchar *image_format;
	if (g_ascii_strcasecmp(mime_type, "image/png") == 0) {
		image_format = "PNG";
	}
	else if (g_ascii_strcasecmp(mime_type, "image/jpeg") == 0) {
		image_format = "JPG";
	}
	else {
		GST_WARNING("Unsupported image type %s", mime_type);
		return NULL;
	}


	ID3_Frame *frame = new ID3_Frame(id);

	ID3_Field *field = frame->GetField(ID3FN_IMAGEFORMAT);
	field->Set(image_format);
	
	field = frame->GetField(ID3FN_MIMETYPE);
	field->Set(mime_type);
	
	// The picture types are taken from taglib/gstid3v2mux.cc
	field = frame->GetField(ID3FN_PICTURETYPE);
	if (g_ascii_strcasecmp(tag, GST_TAG_PREVIEW_IMAGE) == 0) {
		field->Set(ID3PT_OTHER);
	}
	else {
		field->Set(ID3PT_PNG32ICON);
	}

	// The image description is also taken from taglib/gstid3v2mux.cc
	// NOTE: This seems wrong as there's no description in the image.
	const gchar *description = gst_structure_get_string(structure, "image-description");
	if (description) {
		tags_set_frame_field_to_unicode(frame, ID3FN_DESCRIPTION, description);
	}

	field = frame->GetField(ID3FN_DATA);
	field->Set(
		GST_BUFFER_DATA(image), 
		GST_BUFFER_SIZE(image)
	);
	
	return frame;
}


//
// Converts a GST text tag into an ID3 tag.
// The tag will be written as a string, no matter the tag's type.
//
//
// Parameters:
//   tags: the tags collected so far.
//   tag:  the tag to lookup.
//
// Returns:
//   The value of the tag as a sting or NULL if the tag can't be found.
// 
//
static ID3_Frame* tags_tag_to_frame (
	const GstTagList  *tags, 
	const gchar       *tag,
	const ID3_FrameID id
) {
	
	guint size = gst_tag_list_get_tag_size(tags, tag);
	if (size > 1) {
		GST_WARNING("Tag %s has more than one value (%d), but only one tag will be written", tag, size);
	}
	
	gchar *value = tags_tag_to_string(tags, tag);
	if (value == NULL) {return NULL;}
	
	ID3_Frame *frame = tags_text_to_frame(value, id);
	g_free(value);
	
	return frame;
}


//
// Converts a glib string (UTF-8) into an ID3 frame.
//
//
// Parameters:
//   value: the value of the tag.
//   id:    the ID3 frame ID.
//
// Returns:
//   The corresponding ID3_Frame.
//
static ID3_Frame* tags_text_to_frame (
	const gchar       *value,
	const ID3_FrameID id
) {
	
	ID3_Frame *frame = new ID3_Frame(id);
	tags_set_frame_field_to_unicode(frame, ID3FN_TEXT, value);

	return frame;
}


//
// Sets the value of a frame field to UNICODE (UTF-16). This is a capricious
//  operation as lib3id is not handling properly UTF-8.
//
// Parameters:
//   frame:    the frame on which to operate.
//   field_id: the ID of the field to set as UNICODE.
//   value:    the content to set as an UTF-8 string.
//
// Returns:
//   The corresponding ID3_Frame.
//
static void tags_set_frame_field_to_unicode (
	const ID3_Frame   *frame,
	const ID3_FieldID field_id,
	const gchar       *value
) {
	
	// Set the text value to UTF-16
	unicode_t *utf16 = tags_utils_utf8_to_utf16(value);
	
	// Create a frame for the value and set the encoding to UTF-16
	ID3_Field *field = frame->GetField(field_id);
	field->SetEncoding(ID3TE_UTF16);
	field->Set(utf16);
		
	ID3_Field *encoding = frame->GetField(ID3FN_TEXTENC);
	encoding->Set(ID3TE_UTF16);
	g_free(utf16);
}


//
// Converts an UTF-8 string to UTF-16.
//
// The conversion is made with g_convert.
//
// The UTF-16 string returned is specially made for id3lib which seems to be 
// very picky about UNICODE strings.
//
// Parameters:
//   text: the original string in UTF-8.
//
// Returns:
//   The string in UTF-16, this string must be freed with g_free.
//
static unicode_t* tags_utils_utf8_to_utf16 (
	const gchar *text 
) {
	
	// Perform the conversion
	GError *error = NULL;
	gsize bytes_written = 0;
	gchar *converted = g_convert(
		text, -1, 
		"UTF-16BE", "UTF-8",
		NULL, &bytes_written, 
		&error
	);
	
	// Check that all was perfect
	if (error != NULL) {
		GST_WARNING("Converstion from %s to %s failed: %s", "UTF-8", "UTF-16BE", error->message);
		g_error_free(error);
	}
	
	// Just in case that the conversion returns NULL without an error
	if (converted == NULL) {
		return NULL;
	}
	
	// g_convert seems to end the strings with a single null character but an 
	// UTF-16 string needs two of them. In order to correct this, the string has
	// to be resized to one character more and then it needs to have the extra
	// null character appended.
	converted = (gchar *) g_realloc(converted, bytes_written + 2);
	converted[bytes_written] = 0;
	converted[bytes_written + 1] = 0;

	return (unicode_t *) converted;
}


//
// Returns true if teh given buffer is not empty.
//
// Parameters:
//   buffer: the GST buffer to probe.
//
// Returns:
//   true if the buffer is not empty
// 
//
static gboolean tags_buffer_has_data (
	const GstBuffer *buffer
) {

	return GST_IS_BUFFER(buffer)
		&& GST_BUFFER_SIZE(buffer) > 0
		&& GST_BUFFER_CAPS(buffer) != NULL
		&& !gst_caps_is_empty(GST_BUFFER_CAPS(buffer))
	;
}


gboolean gst_id3v23_mux_plugin_init (GstPlugin *plugin) {
	if (! gst_element_register(plugin, PLUGIN, GST_RANK_NONE, GST_TYPE_ID3V23_MUX)) {
		return FALSE;
	}

	return TRUE;
}
