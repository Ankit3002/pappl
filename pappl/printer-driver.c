//
// Printer driver functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "printer-private.h"


//
// Local functions...
//

static ipp_t	*make_attrs(pappl_driver_data_t *data);


//
// 'papplPrinterGetDriverData()' - Get the current driver data.
//

pappl_driver_data_t *			// O - Driver data or `NULL` if none
papplPrinterGetDriverData(
    pappl_printer_t     *printer,	// I - Printer
    pappl_driver_data_t *data)		// I - Pointer to driver data structure to fill
{
  if (!printer || !printer->driver_name || !data)
  {
    if (data)
      memset(data, 0, sizeof(pappl_driver_data_t));

    return (NULL);
  }

  memcpy(data, &printer->driver_data, sizeof(pappl_driver_data_t));

  return (data);
}


//
// 'papplPrinterGetDriverName()' - Get the current driver name.
//

char *					// O - Driver name or `NULL` for none
papplPrinterGetDriverName(
    pappl_printer_t *printer,		// I - Printer
    char            *buffer,		// I - String buffer
    size_t          bufsize)		// I - Size of string buffer
{
  if (!printer || !printer->driver_name || !buffer || bufsize == 0)
  {
    if (buffer)
      *buffer = '\0';

    return (NULL);
  }

  pthread_rwlock_rdlock(&printer->rwlock);
  strlcpy(buffer, printer->driver_name, bufsize);
  pthread_rwlock_unlock(&printer->rwlock);

  return (buffer);
}


//
// 'papplPrinterSetDriverData()' - Set the driver data.
//
// Note: This function regenerates all of the driver-specific capability
// attributes like "media-col-database", "sides-supported", and so forth.
// Use the corresponding `papplPrinterSet` functions to efficiently change the
// "xxx-default" or "xxx-ready" values.
//

void
papplPrinterSetDriverData(
    pappl_printer_t     *printer,	// I - Printer
    pappl_driver_data_t *data,		// I - Driver data
    ipp_t               *attrs)		// I - Additional capability attributes or `NULL` for none
{
  // TODO: implement me
  // Copy driver data then recreate driver_attrs

  if (!printer || !data)
    return;

  pthread_rwlock_rdlock(&printer->rwlock);

  // Copy driver data to printer
  printer->driver_data = *data;

  // Create printer (capability) attributes based on driver data...
  ippDelete(printer->driver_attrs);
  printer->driver_attrs = make_attrs(data);

  if (attrs)
    ippCopyAttributes(printer->driver_attrs, attrs, 0, NULL, NULL);

  pthread_rwlock_unlock(&printer->rwlock);
}


//
// 'make_attrs()' - Make the capability attributes for the given driver data.
//

static ipp_t *				// O - Driver attributes
make_attrs(pappl_driver_data_t *data)	// I - Driver data
{
  ipp_t			*attrs;		// Driver attributes
  unsigned		bit;		// Current bit value
  int			i, j,		// Looping vars
			num_values;	// Number of values
  const char		*svalues[100];	// String values
  int			ivalues[100];	// Integer values
  ipp_t			*cvalues[PAPPL_MAX_MEDIA * 2];
					// Collection values
  char			fn[32],		// FN (finishings) values
			*ptr;		// Pointer into value
  const char		*prefix;	// Prefix string
  const char		*max_name = NULL,// Maximum size
		    	*min_name = NULL;// Minimum size
  static const int	fnvalues[] =	// "finishings" values
  {
    IPP_FINISHINGS_PUNCH,
    IPP_FINISHINGS_STAPLE,
    IPP_FINISHINGS_TRIM
  };
  static const char * const fnstrings[] =
  {					// "finishing-template" values
    "punch",
    "staple",
    "trim"
  };
  static const char * const job_creation_attributes[] =
  {					// job-creation-attributes-supported values
    "copies",
    "document-format",
    "document-name",
    "ipp-attribute-fidelity",
    "job-name",
    "job-priority",
    "media",
    "media-col",
    "multiple-document-handling",
    "orientation-requested",
    "print-color-mode",
    "print-content-optimize",
    "print-quality",
    "printer-resolution"
  };
  static const char * const media_col[] =
  {					// media-col-supported values
    "media-bottom-margin",
    "media-left-margin",
    "media-right-margin",
    "media-size",
    "media-size-name",
    "media-top-margin"
  };
  static const char * const printer_settable_attributes[] =
  {					// printer-settable-attributes values
    "copies-default",
    "document-format-default",
    "label-mode-configured",
    "label-tear-off-configured",
    "media-col-default",
    "media-col-ready",
    "media-default",
    "media-ready",
    "multiple-document-handling-default",
    "orientation-requested-default",
    "print-color-mode-default",
    "print-content-optimize-default",
    "print-darkness-default",
    "print-quality-default",
    "print-speed-default",
    "printer-darkness-configured",
    "printer-geo-location",
    "printer-location",
    "printer-organization",
    "printer-organizational-unit",
    "printer-resolution-default"
  };


  // Create an empty IPP message for the attributes...
  attrs = ippNew();


  // document-format-supported
  num_values = 0;
  svalues[num_values ++] = "application/octet-stream";

  if (data->format && strcmp(data->format, "application/octet-stream"))
    svalues[num_values ++] = data->format;

#ifdef HAVE_LIBJPEG
  svalues[num_values ++] = "image/jpeg";
#endif // HAVE_LIBJPEG

#ifdef HAVE_LIBPNG
  svalues[num_values ++] = "image/png";
#endif // HAVE_LIBPNG

  svalues[num_values ++] = "image/pwg-raster";
  svalues[num_values ++] = "image/urf";

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_MIMETYPE, "document-format-supported", num_values, NULL, svalues);


  if (data->finishings)
  {
    // Assemble values...
    num_values = 0;
    cvalues[num_values   ] = ippNew();
    ippAddString(cvalues[num_values], IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template", NULL, "none");
    ivalues[num_values   ] = IPP_FINISHINGS_NONE;
    svalues[num_values ++] = "none";

    for (ptr = fn, i = 0, prefix = "FN", bit = PAPPL_FINISHINGS_PUNCH; bit <= PAPPL_FINISHINGS_TRIM; i ++, bit *= 2)
    {
      if (data->finishings & bit)
      {
	cvalues[num_values   ] = ippNew();
	ippAddString(cvalues[num_values], IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template", NULL, fnstrings[i]);
	ivalues[num_values   ] = fnvalues[i];
	svalues[num_values ++] = fnstrings[i];

	snprintf(ptr, sizeof(fn) - (size_t)(ptr - fn), "%s%d", prefix, fnvalues[i]);
	ptr += strlen(ptr);
	prefix = "-";
      }
    }

    // finishing-template-supported
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-template-supported", num_values, NULL, svalues);

    // finishing-col-database
    ippAddCollections(attrs, IPP_TAG_PRINTER, "finishing-col-database", num_values, (const ipp_t **)cvalues);

    // finishing-col-default
    ippAddCollection(attrs, IPP_TAG_PRINTER, "finishing-col-default", cvalues[0]);

    // finishing-col-supported
    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "finishing-col-supported", NULL, "finishing-template");

    // finishings-default
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-default", IPP_FINISHINGS_NONE);

    // finishings-supported
    ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "finishings-supported", num_values, ivalues);

    for (i = 0; i < num_values; i ++)
      ippDelete(cvalues[i]);
  }
  else
    fn[0] = '\0';


  // identify-actions-default
  for (num_values = 0, bit = PAPPL_IDENTIFY_ACTIONS_DISPLAY; bit <= PAPPL_IDENTIFY_ACTIONS_SPEAK; bit *= 2)
  {
    if (data->identify_default & bit)
      svalues[num_values ++] = _papplIdentifyActionsString(bit);
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-default", num_values, NULL, svalues);


  // identify-actions-supported
  for (num_values = 0, bit = PAPPL_IDENTIFY_ACTIONS_DISPLAY; bit <= PAPPL_IDENTIFY_ACTIONS_SPEAK; bit *= 2)
  {
    if (data->identify_supported & bit)
      svalues[num_values ++] = _papplIdentifyActionsString(bit);
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "identify-actions-supported", num_values, NULL, svalues);


  // job-creation-attributes-supported
  memcpy(svalues, job_creation_attributes, sizeof(job_creation_attributes));
  num_values = (int)(sizeof(job_creation_attributes) / sizeof(job_creation_attributes[0]));

  if (data->darkness_supported)
    svalues[num_values ++] = "print-darkness";

  if (data->speed_supported[1])
    svalues[num_values ++] = "print-speed";

  for (i = 0; i < data->num_vendor; i ++)
    svalues[num_values ++] = data->vendor[i];

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-creation-attributes-supported", num_values, NULL, svalues);


  // label-mode-supported
  for (num_values = 0, bit = PAPPL_LABEL_MODE_APPLICATOR; bit <= PAPPL_LABEL_MODE_TEAR_OFF; bit *= 2)
  {
    if (data->mode_supported & bit)
      svalues[num_values ++] = _papplLabelModeString(bit);
  }

  if (num_values > 0)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "label-mode-supported", num_values, NULL, svalues);


  // label-tear-offset-supported
  if (data->tear_offset_supported[0] || data->tear_offset_supported[1])
    ippAddRange(attrs, IPP_TAG_PRINTER, "label-tear-offset-supported", data->tear_offset_supported[0], data->tear_offset_supported[1]);


  // media-bottom-margin-supported
  num_values = 0;
  if (data->borderless)
    ivalues[num_values ++] = 0;
  ivalues[num_values ++] = data->bottom_top;

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-bottom-margin-supported", num_values, ivalues);


  // media-col-database
  for (i = 0, num_values = 0; i < data->num_media; i ++)
  {
    if (!strncmp(data->media[i], "custom_max_", 11) || !strncmp(data->media[i], "roll_max_", 9))
    {
      max_name = data->media[i];
    }
    else if (!strncmp(data->media[i], "custom_min_", 11) || !strncmp(data->media[i], "roll_min_", 9))
    {
      min_name = data->media[i];
    }
    else
    {
      pappl_media_col_t	col;		// Media collection
      pwg_media_t	*pwg;		// PWG media size info

      memset(&col, 0, sizeof(col));
      strlcpy(col.size_name, data->media[i], sizeof(col.size_name));
      if ((pwg = pwgMediaForPWG(data->media[i])) != NULL)
      {
	col.size_width  = pwg->width;
	col.size_length = pwg->length;
      }

      if (data->borderless && data->bottom_top > 0 && data->left_right > 0)
	cvalues[num_values ++] = _papplMediaColExport(&col, true);

      col.bottom_margin = col.top_margin = data->bottom_top;
      col.left_margin = col.right_margin = data->left_right;

      cvalues[num_values ++] = _papplMediaColExport(&col, true);
    }
  }

  if (min_name && max_name)
  {
    pwg_media_t	*pwg,			// Current media size info
		max_pwg,		// PWG maximum media size info
		min_pwg;		// PWG minimum media size info
    ipp_t	*col;			// media-size collection

    if ((pwg = pwgMediaForPWG(max_name)) != NULL)
      max_pwg = *pwg;
    else
      memset(&max_pwg, 0, sizeof(max_pwg));

    if ((pwg = pwgMediaForPWG(min_name)) != NULL)
      min_pwg = *pwg;
    else
      memset(&min_pwg, 0, sizeof(min_pwg));

    col = ippNew();
    ippAddRange(col, IPP_TAG_PRINTER, "x-dimension", min_pwg.width, max_pwg.width);
    ippAddRange(col, IPP_TAG_PRINTER, "y-dimension", min_pwg.length, max_pwg.length);

    cvalues[num_values] = ippNew();
    ippAddCollection(cvalues[num_values ++], IPP_TAG_PRINTER, "media-size", col);
    ippDelete(col);
  }

  if (num_values > 0)
  {
    ippAddCollections(attrs, IPP_TAG_PRINTER, "media-col-database", num_values, (const ipp_t **)cvalues);
    for (i = 0; i < num_values; i ++)
      ippDelete(cvalues[i]);
  }

  // media-col-supported
  memcpy(svalues, media_col, sizeof(media_col));
  num_values = (int)(sizeof(media_col) / sizeof(media_col[0]));

  if (data->num_source)
    svalues[num_values ++] = "media-source";

  if (data->top_offset_supported[1])
    svalues[num_values ++] = "media-top-offset";

  if (data->tracking_supported)
    svalues[num_values ++] = "media-tracking";

  if (data->num_type)
    svalues[num_values ++] = "media-type";

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-col-supported", num_values, NULL, svalues);


  // media-left-margin-supported
  num_values = 0;
  if (data->borderless)
    ivalues[num_values ++] = 0;
  ivalues[num_values ++] = data->left_right;

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-left-margin-supported", num_values, ivalues);


  // media-right-margin-supported
  num_values = 0;
  if (data->borderless)
    ivalues[num_values ++] = 0;
  ivalues[num_values ++] = data->left_right;

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-right-margin-supported", num_values, ivalues);


  // media-size-supported
  for (i = 0, num_values = 0; i < data->num_media; i ++)
  {
    pwg_media_t	*pwg;			// PWG media size info

    if (!strncmp(data->media[i], "custom_max_", 11) || !strncmp(data->media[i], "roll_max_", 9))
    {
      max_name = data->media[i];
    }
    else if (!strncmp(data->media[i], "custom_min_", 11) || !strncmp(data->media[i], "roll_min_", 9))
    {
      min_name = data->media[i];
    }
    else if ((pwg = pwgMediaForPWG(data->media[i])) != NULL)
    {
      cvalues[num_values] = ippNew();
      ippAddInteger(cvalues[num_values], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "x-dimension", pwg->width);
      ippAddInteger(cvalues[num_values ++], IPP_TAG_PRINTER, IPP_TAG_INTEGER, "y-dimension", pwg->length);
    }
  }

  if (min_name && max_name)
  {
    pwg_media_t	*pwg,			// Current media size info
		max_pwg,		// PWG maximum media size info
		min_pwg;		// PWG minimum media size info

    if ((pwg = pwgMediaForPWG(max_name)) != NULL)
      max_pwg = *pwg;
    else
      memset(&max_pwg, 0, sizeof(max_pwg));

    if ((pwg = pwgMediaForPWG(min_name)) != NULL)
      min_pwg = *pwg;
    else
      memset(&min_pwg, 0, sizeof(min_pwg));

    cvalues[num_values] = ippNew();
    ippAddRange(cvalues[num_values], IPP_TAG_PRINTER, "x-dimension", min_pwg.width, max_pwg.width);
    ippAddRange(cvalues[num_values ++], IPP_TAG_PRINTER, "y-dimension", min_pwg.length, max_pwg.length);
  }

  if (num_values > 0)
  {
    ippAddCollections(attrs, IPP_TAG_PRINTER, "media-size-supported", num_values, (const ipp_t **)cvalues);
    for (i = 0; i < num_values; i ++)
      ippDelete(cvalues[i]);
  }


  // media-source-supported
  if (data->num_source)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-source-supported", data->num_source, NULL, data->source);


  // media-supported
  if (data->num_media)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-supported", data->num_media, NULL, data->media);


  // media-top-margin-supported
  num_values = 0;
  if (data->borderless)
    ivalues[num_values ++] = 0;
  ivalues[num_values ++] = data->bottom_top;

  ippAddIntegers(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "media-top-margin-supported", num_values, ivalues);


  // media-top-offset-supported
  if (data->top_offset_supported[1])
    ippAddRange(attrs, IPP_TAG_PRINTER, "media-top-offset-supported", data->top_offset_supported[0], data->top_offset_supported[1]);


  // media-tracking-supported
  if (data->tracking_supported)
  {
    for (num_values = 0, bit = PAPPL_MEDIA_TRACKING_CONTINUOUS; bit <= PAPPL_MEDIA_TRACKING_WEB; bit *= 2)
    {
      if (data->tracking_supported & bit)
        svalues[num_values ++] = _papplMediaTrackingString(bit);
    }

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-tracking-supported", num_values, NULL, svalues);
  }


  // media-type-supported
  if (data->num_type)
    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "media-type-supported", data->num_type, NULL, data->type);


  // print-color-mode-default
// TODO: print-color-mode-default
// ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-default", NULL, "monochrome");


  // print-color-mode-supported
// TODO: print-color-mode-supported
// ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-color-mode-supported", (int)(sizeof(print_color_mode) / sizeof(print_color_mode[0])), NULL, print_color_mode);


  // print-darkness-supported
  if (data->darkness_supported)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "print-darkness-supported", 2 * data->darkness_supported);


  // print-speed-supported
  if (data->speed_supported[1])
    ippAddRange(attrs, IPP_TAG_PRINTER, "print-speed-supported", data->speed_supported[0], data->speed_supported[1]);


  // printer-darkness-supported
  if (data->darkness_supported)
    ippAddInteger(attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-darkness-supported", data->darkness_supported);


  // printer-kind
// TODO: printer-kind
// ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-kind", (int)(sizeof(printer_kind) / sizeof(printer_kind[0])), NULL, printer_kind);


  // printer-make-and-model
  ippAddString(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "printer-make-and-model", NULL, data->make_and_model);


  // printer-resolution-supported
  if (data->num_resolution > 0)
    ippAddResolutions(attrs, IPP_TAG_PRINTER, "printer-resolution-supported", data->num_resolution, IPP_RES_PER_INCH, data->x_resolution, data->y_resolution);


  // printer-settable-attributes
  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "printer-settable-attributes", (int)(sizeof(printer_settable_attributes) / sizeof(printer_settable_attributes[0])), NULL, printer_settable_attributes);


  // pwg-raster-document-resolution-supported
  if (data->num_resolution > 0)
    ippAddResolutions(attrs, IPP_TAG_PRINTER, "pwg-raster-document-resolution-supported", data->num_resolution, IPP_RES_PER_INCH, data->x_resolution, data->y_resolution);


  // pwg-raster-document-sheet-back
  if (data->duplex)
  {
    static const char * const backs[] =	// "pwg-raster-document-sheet-back" values
    {
      "normal",
      "flipped",
      "rotated",
      "manual-tumble"
    };

    ippAddString(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-sheet-back", NULL, backs[data->duplex - 1]);
  }


  // pwg-raster-document-type-supported
  for (num_values = 0, bit = PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_8; bit <= PAPPL_PWG_RASTER_TYPE_SRGB_16; bit *= 2)
  {
    if (data->raster_types & bit)
      svalues[num_values ++] = _papplRasterTypeString(bit);
  }

  ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pwg-raster-document-types-supported", num_values, NULL, svalues);


  // urf-supported
  if (data->num_resolution > 0)
  {
    char	dm[32],			// DM (duplex mode) value
		is[256],		// IS (media-source) values
		mt[256],		// MT (media-type) values
		ob[256],		// OB (output-bin) values
		rs[32];			// RS (resolution) values

    num_values = 0;
    svalues[num_values ++] = "V1.4";
    svalues[num_values ++] = "W8";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_SRGB_8)
      svalues[num_values ++] = "SRGB8";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_16)
      svalues[num_values ++] = "ADOBERGB24-48";
    else if (data->raster_types & PAPPL_PWG_RASTER_TYPE_ADOBE_RGB_8)
      svalues[num_values ++] = "ADOBERGB24";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_BLACK_16)
      svalues[num_values ++] = "DEVW8-16";
    else if (data->raster_types & PAPPL_PWG_RASTER_TYPE_BLACK_8)
      svalues[num_values ++] = "DEVW8";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_RGB_16)
      svalues[num_values ++] = "DEVRGB24-48";
    else if (data->raster_types & PAPPL_PWG_RASTER_TYPE_RGB_8)
      svalues[num_values ++] = "DEVRGB24";
    if (data->raster_types & PAPPL_PWG_RASTER_TYPE_CMYK_16)
      svalues[num_values ++] = "DEVCMYK32-64";
    else if (data->raster_types & PAPPL_PWG_RASTER_TYPE_CMYK_8)
      svalues[num_values ++] = "DEVCMYK32";
    svalues[num_values ++] = "PQ3-4-5";

    if (data->duplex)
    {
      snprintf(dm, sizeof(dm), "DM%d", (int)data->duplex);
      svalues[num_values ++] = dm;
    }

    if (fn[0])
      svalues[num_values ++] = fn;

    if (data->num_source)
    {
      static const char * const iss[] =	// IS/"media-source" values
      {
        "auto",
        "main",
        "alternate",
        "large-capacity",
        "manual",
        "envelope",
        "disc",
        "photo",
        "hagaki",
        "main-roll",
        "alternate-roll",
        "top",
        "middle",
        "bottom",
        "side",
        "left",
        "right",
        "center",
        "rear",
        "by-pass-tray",			// a.k.a. multi-purpose tray
        "tray-1",
        "tray-2",
        "tray-3",
        "tray-4",
        "tray-5",
        "tray-6",
        "tray-7",
        "tray-8",
        "tray-9",
        "tray-10",
        "tray-11",
        "tray-12",
        "tray-13",
        "tray-14",
        "tray-15",
        "tray-16",
        "tray-17",
        "tray-18",
        "tray-19",
        "tray-20",
        "roll-1",
        "roll-2",
        "roll-3",
        "roll-4",
        "roll-5",
        "roll-6",
        "roll-7",
        "roll-8",
        "roll-9",
        "roll-10"
      };

      for (i = 0, ptr = is, *ptr = '\0', prefix = "IS"; i < data->num_source; i ++)
      {
        for (j = 0; j < (int)(sizeof(iss) / sizeof(iss[0])); j ++)
        {
          if (!strcmp(iss[j], data->source[i]))
          {
            snprintf(ptr, sizeof(is) - (size_t)(ptr - is), "%s%d", prefix, j);
            ptr += strlen(ptr);
            prefix = "-";
          }
        }
      }

      if (is[0])
        svalues[num_values ++] = is;
    }

    if (data->num_type)
    {
      static const char * const mts[] =	// MT/"media-type" values
      {
        "auto",
        "stationery",
        "transparency",
        "envelope",
        "cardstock",
        "labels",
        "stationery-letterhead",
        "disc",
        "photographic-matte",
        "photographic-satin",
        "photographic-semi-gloss",
        "photographic-glossy",
        "photographic-high-gloss",
        "other"
      };

      for (i = 0, ptr = mt, *ptr = '\0', prefix = "MT"; i < data->num_type; i ++)
      {
        for (j = 0; j < (int)(sizeof(mts) / sizeof(mts[0])); j ++)
        {
          if (!strcmp(mts[j], data->type[i]))
          {
            snprintf(ptr, sizeof(mt) - (size_t)(ptr - mt), "%s%d", prefix, j);
            ptr += strlen(ptr);
            prefix = "-";
          }
        }
      }

      if (mt[0])
        svalues[num_values ++] = mt;
    }

    if (data->num_bin)
    {
      static const char * const obs[] =	// OB/"output-bin" values
      {
        "auto",
        "top",
        "middle",
        "bottom",
        "side",
        "left",
        "right",
        "center",
        "rear",
        "face-up",
        "face-down",
        "large-capacity",
        "stacker",
        "my-mailbox",
        "mailbox-1",
        "mailbox-2",
        "mailbox-3",
        "mailbox-4",
        "mailbox-5",
        "mailbox-6",
        "mailbox-7",
        "mailbox-8",
        "mailbox-9",
        "mailbox-10",
        "stacker-1",
        "stacker-2",
        "stacker-3",
        "stacker-4",
        "stacker-5",
        "stacker-6",
        "stacker-7",
        "stacker-8",
        "stacker-9",
        "stacker-10",
        "tray-1",
        "tray-2",
        "tray-3",
        "tray-4",
        "tray-5",
        "tray-6",
        "tray-7",
        "tray-8",
        "tray-9",
        "tray-10"
      };

      for (i = 0, ptr = ob, *ptr = '\0', prefix = "OB"; i < data->num_bin; i ++)
      {
        for (j = 0; j < (int)(sizeof(obs) / sizeof(obs[0])); j ++)
        {
          if (!strcmp(obs[j], data->bin[i]))
          {
            snprintf(ptr, sizeof(ob) - (size_t)(ptr - ob), "%s%d", prefix, j);
            ptr += strlen(ptr);
            prefix = "-";
          }
        }
      }

      if (ob[0])
        svalues[num_values ++] = ob;
    }

    if (data->input_face_up)
      svalues[num_values ++] = "IFU0";

    if (data->output_face_up)
      svalues[num_values ++] = "OFU0";

    if (data->num_resolution == 1)
      snprintf(rs, sizeof(rs), "RS%d", data->x_resolution[0]);
    else
      snprintf(rs, sizeof(rs), "RS%d-%d", data->x_resolution[data->num_resolution - 2], data->x_resolution[data->num_resolution - 1]);

    svalues[num_values ++] = rs;

    ippAddStrings(attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "urf-supported", num_values, NULL, svalues);
  }

  return (attrs);
}