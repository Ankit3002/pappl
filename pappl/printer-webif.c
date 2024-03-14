//
// Printer web interface functions for the Printer Application Framework
//
// Copyright © 2019-2024 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// Local functions...
//

static void	job_cb(pappl_job_t *job, pappl_client_t *client);
static void	job_pager(pappl_client_t *client, pappl_printer_t *printer, int job_index, int limit);
static char	*localize_keyword(pappl_client_t *client, const char *attrname, const char *keyword, char *buffer, size_t bufsize);
static char	*localize_media(pappl_client_t *client, pappl_media_col_t *media, bool include_source, char *buffer, size_t bufsize);
static void	media_chooser(pappl_client_t *client, pappl_pr_driver_data_t *driver_data, const char *title, const char *name, pappl_media_col_t *media);
static char	*time_string(pappl_client_t *client, time_t tv, char *buffer, size_t bufsize);


//
// '_papplPrinterWebCancelAllJobs()' - Cancel all printer jobs.
//

void
_papplPrinterWebCancelAllJobs(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  const char	*status = NULL;		// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else
    {
      char	path[1024];		// Resource path

      papplPrinterCancelAllJobs(printer);
      snprintf(path, sizeof(path), "%s/jobs", printer->uriname);
      papplClientRespondRedirect(client, HTTP_STATUS_FOUND, path);
      cupsFreeOptions(num_form, form);
      return;
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Cancel All Jobs"), 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPrintf(client, "           <input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Confirm Cancel All")));

  if (papplPrinterGetNumberOfActiveJobs(printer) > 0)
  {
    papplClientHTMLPrintf(client,
			  "          <table class=\"list\" summary=\"Jobs\">\n"
			  "            <thead>\n"
			  "              <tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th></th></tr>\n"
			  "            </thead>\n"
			  "            <tbody>\n", papplClientGetLocString(client, _PAPPL_LOC("Job #")), papplClientGetLocString(client, _PAPPL_LOC("Name")), papplClientGetLocString(client, _PAPPL_LOC("Owner")), papplClientGetLocString(client, _PAPPL_LOC("Pages Completed")), papplClientGetLocString(client, _PAPPL_LOC("Status")));

    papplPrinterIterateActiveJobs(printer, (pappl_job_cb_t)job_cb, client, 1, 0);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");
  }
  else
    papplClientHTMLPrintf(client, "        <p>%s</p>\n", papplClientGetLocString(client, _PAPPL_LOC("No jobs in history.")));

  papplClientHTMLFooter(client);
}


//
// '_papplPrinterWebConfig()' - Show the printer configuration web page.
//

void
_papplPrinterWebConfig(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  const char	*status = NULL;		// Status message, if any
  char		dns_sd_name[64],	// DNS-SD name
		location[128],		// Location
		geo_location[128],	// Geo-location latitude
		organization[128],	// Organization
		org_unit[128];		// Organizational unit
  pappl_contact_t contact;		// Contact info


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else
    {
      _papplPrinterWebConfigFinalize(printer, num_form, form);

      status = _PAPPL_LOC("Changes saved.");
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Configuration"), 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  _papplClientHTMLInfo(client, true, papplPrinterGetDNSSDName(printer, dns_sd_name, sizeof(dns_sd_name)), papplPrinterGetLocation(printer, location, sizeof(location)), papplPrinterGetGeoLocation(printer, geo_location, sizeof(geo_location)), papplPrinterGetOrganization(printer, organization, sizeof(organization)), papplPrinterGetOrganizationalUnit(printer, org_unit, sizeof(org_unit)), papplPrinterGetContact(printer, &contact));

  papplClientHTMLPrinterFooter(client);
}


//
// '_papplPrinterWebConfigFinalize()' - Save the changes to the printer configuration.
//

void
_papplPrinterWebConfigFinalize(
    pappl_printer_t *printer,		// I - Printer
    cups_len_t      num_form,		// I - Number of form variables
    cups_option_t   *form)		// I - Form variables
{
  const char	*value,			// Form value
		*geo_lat,		// Geo-location latitude
		*geo_lon,		// Geo-location longitude
		*contact_name,		// Contact name
		*contact_email,		// Contact email
		*contact_tel;		// Contact telephone number


  if ((value = cupsGetOption("dns_sd_name", num_form, form)) != NULL)
    papplPrinterSetDNSSDName(printer, *value ? value : NULL);

  if ((value = cupsGetOption("location", num_form, form)) != NULL)
    papplPrinterSetLocation(printer, *value ? value : NULL);

  geo_lat = cupsGetOption("geo_location_lat", num_form, form);
  geo_lon = cupsGetOption("geo_location_lon", num_form, form);
  if (geo_lat && geo_lon)
  {
    char	uri[1024];		// "geo:" URI

    if (*geo_lat && *geo_lon)
    {
      snprintf(uri, sizeof(uri), "geo:%g,%g", strtod(geo_lat, NULL), strtod(geo_lon, NULL));
      papplPrinterSetGeoLocation(printer, uri);
    }
    else
      papplPrinterSetGeoLocation(printer, NULL);
  }

  if ((value = cupsGetOption("organization", num_form, form)) != NULL)
    papplPrinterSetOrganization(printer, *value ? value : NULL);

  if ((value = cupsGetOption("organizational_unit", num_form, form)) != NULL)
    papplPrinterSetOrganizationalUnit(printer, *value ? value : NULL);

  contact_name  = cupsGetOption("contact_name", num_form, form);
  contact_email = cupsGetOption("contact_email", num_form, form);
  contact_tel   = cupsGetOption("contact_telephone", num_form, form);
  if (contact_name || contact_email || contact_tel)
  {
    pappl_contact_t	contact;	// Contact info

    memset(&contact, 0, sizeof(contact));

    if (contact_name)
      papplCopyString(contact.name, contact_name, sizeof(contact.name));
    if (contact_email)
      papplCopyString(contact.email, contact_email, sizeof(contact.email));
    if (contact_tel)
      papplCopyString(contact.telephone, contact_tel, sizeof(contact.telephone));

    papplPrinterSetContact(printer, &contact);
  }
}



//'_papplPrinterPreset' - Show the presets homepage.
//
void 
_papplPrinterPreset(
  pappl_client_t *client,
  pappl_printer_t *printer
)
{
  // take out job-preset-supported from printer->attrs over here...

    if (!papplClientHTMLAuthorize(client))
    return;

  papplClientHTMLPrinterHeader(client , printer , "Presets", 0 , NULL, NULL);
  int i, count;
  char *uri = printer->uriname;
  // http://localhost:8000/canon/presets/shit/edit
  char	edit_button[1024]; 
  char  create_button[1024];
  char  copy_button[1024];
  char  delete_button[1024];

  char form_link[1024];
  snprintf(create_button, sizeof(create_button), "%s/presets/create", uri);

  // define the route to enter preset page for creating preset page ...
  papplClientHTMLPrintf(client ,"<table>");
  papplClientHTMLPrintf(client, "<button id=\"create_button\" onClick=\"window.location.href = '%s';\">Create</button>" ,create_button);

  count = cupsArrayGetCount(printer->presets);
  for(i=0; i< count; i++)
  {
    pappl_pr_preset_data_t *preset = cupsArrayGetElement(printer->presets , i);
    snprintf(edit_button, sizeof(edit_button), "%s/presets/%s/edit?name=%s", uri, preset->name,preset->name);
    snprintf(copy_button, sizeof(copy_button), "%s/presets/%s/copy?name=%s", uri, preset->name,preset->name);
    snprintf(delete_button, sizeof(delete_button), "%s/presets/%s/delete?name=%s", uri, preset->name, preset->name);

    // resources are already created over here...
    papplClientHTMLPrintf(client , "<tr><td> %s </td><td>   <button id=\"edit_button\" onClick=\"window.location.href = '%s';\">Edit</button>    </td> <td> <button id=\"copy_button\" onClick=\"window.location.href = '%s';\">Copy</button>  </td> <td>  <button id=\"delete_button\" onClick=\"window.location.href = '%s';\">Delete</button>  </td> </tr>" , preset->name , edit_button, copy_button, delete_button);
  }
  papplClientHTMLPrintf(client, "</table>");

}




//
// '_papplPrinterPresetDelete' - Show the preset Deletion page.
//
void
_papplPrinterPresetDelete(
    pappl_client_t  *client,		// I - Client
    resource_data_t *resource_data)		// I - Printer
{
  const char	*status = NULL;		// Status message, if any
  pappl_printer_t * printer = resource_data->printer;
  char *uri = printer->uriname;
  char *preset_name = resource_data->preset_name;
  char buffer[1024];
  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    int		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (int)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if (printer->processing_job)
    {
      // Printer is processing a job...
      status = _PAPPL_LOC("Printer is currently active.");
    }
    else
    {
      if (!printer->is_deleted)
      {
        // pick the preset from here ...
        
        // here call the method to delete the preset from the memory ...
        // traverse the preset that you wanted to delete ...
        // papplPresetDelete();
          int preset_iterator , preset_count;
          preset_count = cupsArrayGetCount(printer->presets);
          pappl_pr_preset_data_t * iterator_preset;
          for(preset_iterator = 0;preset_iterator < preset_count; preset_iterator++)
          {
            iterator_preset = cupsArrayGetElement(printer->presets, preset_iterator);
            if(!strcasecmp(iterator_preset->name , preset_name))
              break;
          }
        papplPresetDelete(printer, iterator_preset);
        // papplPrinterDelete(printer);
        // printer = NULL;
        
      }
      snprintf(buffer, sizeof(buffer), "%s/presets", uri);
      papplClientRespondRedirect(client, HTTP_STATUS_FOUND, buffer);
      cupsFreeOptions(num_form, form);
      return;
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Delete Preset"), 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPrintf(client,"          <input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Confirm Delete Preset")));

  papplClientHTMLFooter(client);
}


//
// '_papplPrinterPresetCopy' - Show the preset Copy Page.
//
void _papplPrinterPresetCopy(
    pappl_client_t  *client,		// I - Client
    resource_data_t *resource_data
    )		
{


  int			i, j;		// Looping vars


  pappl_pr_driver_data_t data;		// Driver data


  const char		*keyword;	// Current keyword
  char			text[256];	// Localized text for keyword
  const char		*status = NULL;	// Status message, if any
  bool			show_source = false;
					// Show the media source?
  static const char * const orients[] =	// orientation-requested strings
  {
    _PAPPL_LOC("Portrait"),
    _PAPPL_LOC("Landscape"),
    _PAPPL_LOC("Reverse Landscape"),
    _PAPPL_LOC("Reverse Portrait"),
    _PAPPL_LOC("Auto")
  };
  static const char * const orient_svgs[] =
  {					// orientation-requested images
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='18' font-size='18' fill='currentColor' rotate='0'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='19' font-size='18' fill='currentColor' rotate='-90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='6' font-size='18' fill='currentColor' rotate='90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='7' font-size='18' fill='currentColor' rotate='180'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='5' y='18' font-size='18' fill='currentColor' rotate='0'%3e?%3c/text%3e%3c/svg%3e"
  };

  static const char * const static_attribute_names[] = 
  {
     "orientation-requested" ,
     "print-color-mode", "sides", "output-bin", 
     "print-quality", "print-darkness", "print-speed",
     "print-content-optimize","print-scaling",
     "print-resolution"
  };


  // checking whether we have authorized client or not ....

  if (!papplClientHTMLAuthorize(client))
    return;


  pappl_printer_t * printer = resource_data->printer;
  const char *preset_name = resource_data->preset_name;


  // get all the driver data over here ...
  papplPrinterGetDriverData(printer, &data);

  // write the logic to grab particular preset ...
  int preset_iterator , preset_count;
  preset_count = cupsArrayGetCount(printer->presets);
  pappl_pr_preset_data_t * iterator_preset;
  for(preset_iterator = 0;preset_iterator < preset_count; preset_iterator++)
  {
    iterator_preset = cupsArrayGetElement(printer->presets, preset_iterator);
    if(!strcasecmp(iterator_preset->name , preset_name))
      break;
  }

  
  
  if (client->operation == HTTP_STATE_POST)
  {

    // instance variables ...
    int		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    int			num_vendor = 0;	// Number of vendor options
    cups_option_t	*vendor = NULL;	// Vendor options

    if ((num_form = (int)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }

      // after fetching the data from the web forms .... ( here add those into the driver ... ( saving data ))  
    else
    {

      // create local variables ...

      // write the code over here to save presets in ipp_t object ...






      pappl_pr_preset_data_t *preset = calloc(1 , sizeof(pappl_pr_preset_data_t));

      preset->driver_attrs = ippNew();

      const char	*value;		// Value of form variable
      char		*end;			// End of value


      preset->preset_id = preset_count + 1;


      // set the the data into preset .... ( The standard one ...)
      if((value = cupsGetOption("preset_name", num_form , form)) != NULL)
      {
        preset->name = strdup(value);

        // preset->name = strdup(value);
        // int preset_iterator , preset_count;
        int flag = 0;
        // preset_count = cupsArrayGetCount(printer->presets);
        // pappl_pr_preset_data_t * iterator_preset;
        for(preset_iterator = 0;preset_iterator < preset_count; preset_iterator++)
        {
          iterator_preset = cupsArrayGetElement(printer->presets, preset_iterator);
          if(!strcasecmp(iterator_preset->name , preset->name))
          {
            flag = 1;
            break;
          }
        }

          if(flag)
          {
            free(preset->name);
            status = _PAPPL_LOC("Preset With the same name already exits.");
          }
          else
          {
            if ((value = cupsGetOption("orientation-requested", num_form, form)) != NULL)
            {
              preset->orient_default_check = true;
              preset->orient_default = (ipp_orient_t)strtol(value, &end, 10);

              if (errno == ERANGE || *end || preset->orient_default < IPP_ORIENT_PORTRAIT || preset->orient_default > IPP_ORIENT_NONE)
                preset->orient_default = IPP_ORIENT_PORTRAIT;
            }

            if ((value = cupsGetOption("output-bin", num_form, form)) != NULL)
            {
              preset->bin_default_check = true;
              for (i = 0; i < preset->num_bin; i ++)
              {
                if (!strcmp(preset->bin[i], value))
                {
                  preset->bin_default = i;
                  break;
                }
        }
            }

            if ((value = cupsGetOption("print-color-mode", num_form, form)) != NULL)
            {
              preset->color_default_check = true;
              preset->color_default = _papplColorModeValue(value);
            }

            if ((value = cupsGetOption("print-content-optimize", num_form, form)) != NULL)
            {
              preset->content_default_check = true;
              preset->content_default = _papplContentValue(value);
            }

            if ((value = cupsGetOption("print-darkness", num_form, form)) != NULL)
            {
              preset->darkness_configured_check = true;
              preset->darkness_configured = (int)strtol(value, &end, 10);

              if (errno == ERANGE || *end || preset->darkness_configured < 0 || preset->darkness_configured > 100)
                preset->darkness_configured = 50;
            }

            if ((value = cupsGetOption("print-quality", num_form, form)) != NULL)
            {
              preset->quality_defualt_check =true;
              preset->quality_default = (ipp_quality_t)ippEnumValue("print-quality", value);

            }

            if ((value = cupsGetOption("print-scaling", num_form, form)) != NULL)
            {
              preset->scaling_default_check = true;
              preset->scaling_default = _papplScalingValue(value);

            }

            if ((value = cupsGetOption("print-speed", num_form, form)) != NULL)
            {
              preset->speed_defualt_check = true;
              preset->speed_default = (int)strtol(value, &end, 10) * 2540;

              if (errno == ERANGE || *end || preset->speed_default < 0 || preset->speed_default > preset->speed_supported[1])
                preset->speed_default = 0;
            }

            if ((value = cupsGetOption("sides", num_form, form)) != NULL)
            {
              preset->sides_default_check = true;
              preset->sides_default = _papplSidesValue(value);

            }

            if ((value = cupsGetOption("printer-resolution", num_form, form)) != NULL)
            {
              preset->x_default_check = true;
              preset->y_default_check = true;
              if (sscanf(value, "%dx%ddpi", &preset->x_default, &preset->y_default) == 1)
                preset->y_default = preset->x_default;
            }

            if ((value = cupsGetOption("media-source", num_form, form)) != NULL)
            {
              preset->media_default_check = true;
              for (i = 0; i < preset->num_source; i ++)
        {
          if (!strcmp(value, preset->source[i]))
          {
            preset->media_default = preset->media_ready[i];
            break;
          }
        }
            }


            for (i = 0; i < data.num_vendor; i ++)
            {
              char	supattr[128];		// xxx-supported

              snprintf(supattr, sizeof(supattr), "%s-supported", data.vendor[i]);
              // printf("The iterator can make supattr and it's value is ---- %s\n",supattr );
              if(cupsGetOption(data.vendor[i], num_form, form) == NULL)
              {
                preset->is_vendor[i] = false;
              }

              else if((value = cupsGetOption(data.vendor[i], num_form, form)) != NULL)
              {          
                preset->is_vendor[i] = true;
                printf("The if value that get changed in form is ---- %s --- %s\n", value , data.vendor[i]);
                num_vendor = (int)cupsAddOption(data.vendor[i], value, (int)num_vendor, &vendor);

              }


              else if (ippFindAttribute(printer->driver_attrs, supattr, IPP_TAG_BOOLEAN))
              {
                printf("The else value that get changed in form is ---- %s --- %s\n", value , data.vendor[i]);
                num_vendor = (int)cupsAddOption(data.vendor[i], "false", (int)num_vendor, &vendor);
              }
                
            }

            papplPrinterSetPresetsVendor(printer, preset, num_vendor, vendor);

            // add the preset object into the system ....
            if (papplPrinterAddPresetCreate( printer , preset))
              status = _PAPPL_LOC("Changes saved.");
            else
              status = _PAPPL_LOC("Bad printer defaults.");

            cupsFreeOptions((int)num_vendor, vendor);
          }
          }

      }


    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Let's Copy your preset over here..."), 0, NULL, NULL);



  if (status)
  {
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));
  }

// here we are starting the web forms ... to get data ....
  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");




   char		printer_name[128] = "";	// Printer Name


  papplClientHTMLPrintf(client,
    " <tr> <th><label for=\"printer_name\">%s:</label><br>\n </th>"
    "<td> <input type=\"text\" name=\"preset_name\" placeholder=\"%s\" value=\"%s\" required><br> </td></tr>\n",
    papplClientGetLocString(client, _PAPPL_LOC("Name")),
    papplClientGetLocString(client, _PAPPL_LOC("Name of Preset")), iterator_preset->name);

  // media-col-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "media"));

  if(iterator_preset->media_default_check)
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"media-source-checkbox\" checked >");

    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s/media\">%s</a></td></tr>\n", printer->uriname, papplClientGetLocString(client, _PAPPL_LOC("Configure Media")));

  }
  else
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"media-source-checkbox\" >");
    papplClientHTMLPrintf(client, " <a class=\"btn\" disabled href=\"%s/media\">%s</a></td></tr>\n", printer->uriname, papplClientGetLocString(client, _PAPPL_LOC("Configure Media")));
  }
  if (data.num_source > 1)
  {
    papplClientHTMLPuts(client, "<select name=\"media-source\">");

    for (i = 0; i < data.num_source; i ++)
    {
      // See if any two sources have the same size...
      for (j = i + 1; j < data.num_source; j ++)
      {
        if (iterator_preset->media_ready[i].size_width > 0 && iterator_preset->media_ready[i].size_width == iterator_preset->media_ready[j].size_width && iterator_preset->media_ready[i].size_length == iterator_preset->media_ready[j].size_length)
        {
          show_source = true;
          break;
        }
      }
    }

    for (i = 0; i < data.num_source; i ++)
    {
      keyword = data.source[i];

      if (strcmp(keyword, "manual"))
      {
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, !strcmp(keyword, iterator_preset->media_default.source) ? " selected" : "", localize_media(client, iterator_preset->media_ready + i, show_source, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</select>");
  }
  else
    papplClientHTMLEscape(client, localize_media(client, iterator_preset->media_ready, false, text, sizeof(text)), 0);


  // orientation-requested-default

  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "orientation-requested"));
  if(iterator_preset->orient_default_check)
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"orientation-requested-checkbox\" checked >");
    for (i = IPP_ORIENT_PORTRAIT; i <= IPP_ORIENT_NONE; i ++)
    {
      papplClientHTMLPrintf(client, "<label class=\"image\"><input type=\"radio\" name=\"orientation-requested\" value=\"%d\"%s> <img src=\"data:image/svg+xml,%s\" alt=\"%s\"></label> ", i, iterator_preset->orient_default == (ipp_orient_t)i ? " checked" : "", orient_svgs[i - IPP_ORIENT_PORTRAIT], orients[i - IPP_ORIENT_PORTRAIT]);
    }
  }
  else
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"orientation-requested-checkbox\"  >");
    for (i = IPP_ORIENT_PORTRAIT; i <= IPP_ORIENT_NONE; i ++)
    {
      papplClientHTMLPrintf(client, "<label class=\"image\"><input type=\"radio\" disabled name=\"orientation-requested\" value=\"%d\"%s> <img src=\"data:image/svg+xml,%s\" alt=\"%s\"></label> ", i, iterator_preset->orient_default == (ipp_orient_t)i ? " checked" : "", orient_svgs[i - IPP_ORIENT_PORTRAIT], orients[i - IPP_ORIENT_PORTRAIT]);
    }

  } 



  papplClientHTMLPuts(client, "</td></tr>\n");

  // print-color-mode-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-color-mode"));
  if(iterator_preset->color_default_check)
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-color-mode-checkbox\" checked >");
    if (data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME) || data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_AUTO_MONOCHROME))
    {
      papplClientHTMLPuts(client, "B&amp;W");
    }
    else
    {
      for (i = PAPPL_COLOR_MODE_AUTO; i <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; i *= 2)
      {
        if ((data.color_supported & (pappl_color_mode_t)i) && i != PAPPL_COLOR_MODE_AUTO_MONOCHROME)
        {
    keyword = _papplColorModeString((pappl_color_mode_t)i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-color-mode\"  value=\"%s\"%s> %s</label> ", keyword, (pappl_color_mode_t)i == iterator_preset->color_default ? " checked" : "", localize_keyword(client, "print-color-mode", keyword, text, sizeof(text)));
        }
      }
    }
  }
  else
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-color-mode-checkbox\" >");
      if (data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME) || data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_AUTO_MONOCHROME))
      {
        papplClientHTMLPuts(client, "B&amp;W");
      }
      else
      {
        for (i = PAPPL_COLOR_MODE_AUTO; i <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; i *= 2)
        {
          if ((data.color_supported & (pappl_color_mode_t)i) && i != PAPPL_COLOR_MODE_AUTO_MONOCHROME)
          {
      keyword = _papplColorModeString((pappl_color_mode_t)i);
      papplClientHTMLPrintf(client, "<label><input type=\"radio\" disabled name=\"print-color-mode\"  value=\"%s\"%s> %s</label> ", keyword, (pappl_color_mode_t)i == iterator_preset->color_default ? " checked" : "", localize_keyword(client, "print-color-mode", keyword, text, sizeof(text)));
          }
        }
      }

  }


  papplClientHTMLPuts(client, "</td></tr>\n");

  if (data.sides_supported && data.sides_supported != PAPPL_SIDES_ONE_SIDED)
  {
    // sides-default
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "sides"));
    if(iterator_preset->sides_default_check)
    {
      papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"sides-checkbox\" checked >");
      for (i = PAPPL_SIDES_ONE_SIDED; i <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; i *= 2)
      {
        if (data.sides_supported & (pappl_sides_t)i)
        {
    keyword = _papplSidesString((pappl_sides_t)i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"sides\"  value=\"%s\"%s> %s</label> ", keyword, (pappl_sides_t)i == iterator_preset->sides_default ? " checked" : "", localize_keyword(client, "sides", keyword, text, sizeof(text)));
        }
      }
    }
    else
    {
      papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"sides-checkbox\" >");
      for (i = PAPPL_SIDES_ONE_SIDED; i <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; i *= 2)
      {
        if (data.sides_supported & (pappl_sides_t)i)
        {
    keyword = _papplSidesString((pappl_sides_t)i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" disabled name=\"sides\"  value=\"%s\"%s> %s</label> ", keyword, (pappl_sides_t)i == iterator_preset->sides_default ? " checked" : "", localize_keyword(client, "sides", keyword, text, sizeof(text)));
        }
      }

    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // output-bin-default
  if (iterator_preset->num_bin > 0)
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "output-bin"));
    if (iterator_preset->num_bin > 1)
    {
      if(iterator_preset->bin_default_check)
      {
        papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"output-bin-checkbox\" checked >");
        papplClientHTMLPuts(client, "<select name=\"output-bin\" >");
      }
      else
      {
        papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"output-bin-checkbox\" >");
        papplClientHTMLPuts(client, "<select disabled name=\"output-bin\" >");
      }
      for (i = 0; i < iterator_preset->num_bin; i ++)
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", iterator_preset->bin[i], i == iterator_preset->bin_default ? " selected" : "", localize_keyword(client, "output-bin", iterator_preset->bin[i], text, sizeof(text)));
      papplClientHTMLPuts(client, "</select>");
    }
    else
    {
      papplClientHTMLPrintf(client, "%s", localize_keyword(client, "output-bin", iterator_preset->bin[iterator_preset->bin_default], text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // print-quality-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-quality"));
  if(iterator_preset->quality_defualt_check)
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-quality-checkbox\" checked >");
    for (i = IPP_QUALITY_DRAFT; i <= IPP_QUALITY_HIGH; i ++)
    {
      keyword = ippEnumString("print-quality", i);
      papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-quality\"  value=\"%s\"%s> %s</label> ", keyword, (ipp_quality_t)i == iterator_preset->quality_default ? " checked" : "", localize_keyword(client, "print-quality", keyword, text, sizeof(text)));
    }
  }
  else
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-quality-checkbox\" >");
    for (i = IPP_QUALITY_DRAFT; i <= IPP_QUALITY_HIGH; i ++)
    {
      keyword = ippEnumString("print-quality", i);
      papplClientHTMLPrintf(client, "<label><input type=\"radio\" disabled name=\"print-quality\"  value=\"%s\"%s> %s</label> ", keyword, (ipp_quality_t)i == iterator_preset->quality_default ? " checked" : "", localize_keyword(client, "print-quality", keyword, text, sizeof(text)));
    }
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-darkness-configured
  if (data.darkness_supported)
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-darkness"));
    if(iterator_preset->darkness_configured_check)
    {
      papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-darkness-checkbox\" checked >");
      papplClientHTMLPrintf(client, "<select name=\"print-darkness\">");
    }
    else
    {
      papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-darkness-checkbox\" >");
      papplClientHTMLPrintf(client, "<select disabled name=\"print-darkness\">");
    }
    for (i = 0; i < data.darkness_supported; i ++)
    {
      int percent = 100 * i / (data.darkness_supported - 1);
					// Percent darkness

      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d%%</option>", percent, percent == iterator_preset->darkness_configured ? " selected" : "", percent);
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-speed-default
  if (data.speed_supported[1])
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th>", papplClientGetLocString(client, "print-speed"), papplClientGetLocString(client, _PAPPL_LOC("Auto")));
    if(iterator_preset->speed_defualt_check)
    {
      papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-speed-checkbox\" checked>");
      for (i = data.speed_supported[0]; i <= data.speed_supported[1]; i += 2540)
      {
        if (i > 0)
        {
          papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), i > 2540 ? _PAPPL_LOC("%d inches/sec") : _PAPPL_LOC("%d inch/sec"), i / 2540);
    papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%s</option>", i / 2540, i == iterator_preset->speed_default ? " selected" : "", text);
        }
      }
    }
    else
    {
      papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-speed-checkbox\" >");
      for (i = data.speed_supported[0]; i <= data.speed_supported[1]; i += 2540)
      {
        if (i > 0)
        {
          papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), i > 2540 ? _PAPPL_LOC("%d inches/sec") : _PAPPL_LOC("%d inch/sec"), i / 2540);
    papplClientHTMLPrintf(client, "<option disabled value=\"%d\"%s>%s</option>", i / 2540, i == iterator_preset->speed_default ? " selected" : "", text);
        }
      }
    }


    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-content-optimize-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th> <td>", papplClientGetLocString(client, "print-content-optimize"));
  if(iterator_preset->content_default_check)
  {
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-content-optimize-checkbox\" checked >");
    papplClientHTMLPrintf(client, "<select name=\"print-content-optimize\">");
  }
  else
  {
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-content-optimize-checkbox\" >");
    papplClientHTMLPrintf(client, "<select disabled name=\"print-content-optimize\">");
  }

  for (i = PAPPL_CONTENT_AUTO; i <= PAPPL_CONTENT_TEXT_AND_GRAPHIC; i *= 2)
  {
    keyword = _papplContentString((pappl_content_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_content_t)i == iterator_preset->content_default ? " selected" : "", localize_keyword(client, "print-content-optimize", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-scaling-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-scaling"));
  if(iterator_preset->scaling_default_check)
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\"  id=\"print-scaling-checkbox\" checked >");
    papplClientHTMLPrintf(client, "<select name=\"print-scaling\">");

  }
  else
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\"  id=\"print-scaling-checkbox\" >");
    papplClientHTMLPrintf(client, "<select disabled name=\"print-scaling\">");

  }
  for (i = PAPPL_SCALING_AUTO; i <= PAPPL_SCALING_NONE; i *= 2)
  {
    keyword = _papplScalingString((pappl_scaling_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_scaling_t)i == iterator_preset->scaling_default ? " selected" : "", localize_keyword(client, "print-scaling", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // printer-resolution-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "printer-resolution"));
  if(iterator_preset->x_default_check && iterator_preset->y_default_check)
  {
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"printer-resolution-checkbox\" checked>");
    if (data.num_resolution == 1)
    {
      if (data.x_resolution[0] != data.y_resolution[0])
        papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%dx%ddpi")), data.x_resolution[0], data.y_resolution[0]);
      else
        papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%ddpi")), data.x_resolution[0]);
    }
    else
    {
      papplClientHTMLPuts(client, "<select name=\"printer-resolution\">");
      for (i = 0; i < data.num_resolution; i ++)
      {
        if (data.x_resolution[i] != data.y_resolution[i])
          papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%dx%ddpi"), data.x_resolution[i], data.y_resolution[i]);
        else
    papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%ddpi"), data.x_resolution[i]);

        papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", text, (iterator_preset->x_default == data.x_resolution[i] && iterator_preset->y_default == data.y_resolution[i]) ? " selected" : "", text);
      }
      papplClientHTMLPuts(client, "</select>");
    }
  }
  else
  {
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"printer-resolution-checkbox\" >");
    if (data.num_resolution == 1)
    {
      if (data.x_resolution[0] != data.y_resolution[0])
        papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%dx%ddpi")), data.x_resolution[0], data.y_resolution[0]);
      else
        papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%ddpi")), data.x_resolution[0]);
    }
    else
    {
      papplClientHTMLPuts(client, "<select disabled name=\"printer-resolution\">");
      for (i = 0; i < data.num_resolution; i ++)
      {
        if (data.x_resolution[i] != data.y_resolution[i])
          papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%dx%ddpi"), data.x_resolution[i], data.y_resolution[i]);
        else
    papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%ddpi"), data.x_resolution[i]);

        papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", text, (iterator_preset->x_default == data.x_resolution[i] && iterator_preset->y_default == data.y_resolution[i]) ? " selected" : "", text);
      }
      papplClientHTMLPuts(client, "</select>");
    }
  }



  papplClientHTMLPuts(client, "</td></tr>\n");

  // papplClientHTMLPrintf(client , "<script> function setupCheckboxSelectInteraction(checkboxId, selectName) { var checkbox = document.getElementById(checkboxId); var selectElement = document.querySelector('select[name=\"' + selectName + '\"]'); if (checkbox && selectElement) { checkbox.addEventListener('change', function() { selectElement.disabled = !checkbox.checked; }); } } function setupCheckboxRadioInteraction(checkboxId, radioName) { var checkbox = document.getElementById(checkboxId); var radioButtons = document.querySelectorAll('input[type=\"radio\"][name=\"' + radioName + '\"]'); if (checkbox && radioButtons.length > 0) { checkbox.addEventListener('change', function() { radioButtons.forEach(function(radioButton) { radioButton.disabled = !checkbox.checked; }); }); } } </script>");
  papplClientHTMLPrintf(client, "<script> function setup_Interaction(checkboxId, inputName) { var types = [\"select\", \"radio\", \"text\", \"number\", \"checkbox\"]; var checkbox = document.getElementById(checkboxId); types.forEach(function (type) { var createdPattern; switch (type) { case \"select\": createdPattern = 'select[name=\"' + inputName + '\"]'; break; case \"radio\": createdPattern = 'input[type=\"radio\"][name=\"' + inputName + '\"]'; break; case \"text\": created_pattern = 'input[type=\"text\"][name=\"' + inputName + '\"]'; case \"number\": created_pattern = 'input[type=\"number\"][name=\"' + inputName + '\"]'; case \"checkbox\": created_pattern = 'input[type=\"checkbox\"][name=\"' + inputName + '\"]'; } console.log(createdPattern); var inputs = document.querySelectorAll(createdPattern); if (checkbox && inputs.length > 0) { checkbox.addEventListener(\"change\", function () { inputs.forEach(function (input) { input.disabled = !checkbox.checked; }); }); } }); } </script>");




  // Vendor options
  _papplRWLockRead(printer);

  for (i = 0; i < data.num_vendor; i ++)
  {
    // printf("The value inside check array for this %s attribute is --> %d", data.vendor[i], iterator_preset->is_vendor[i]);

    /* initialize the variables and setting the different variables ...
    */
    char	defname[128],		// xxx-default name
		defvalue[1024],		// xxx-default value
		supname[128];		// xxx-supported name
    ipp_attribute_t *defattr,		// xxx-default attribute
	      	*supattr;		// xxx-supported attribute
    int		count;			// Number of values
    char buffer[1024];
    snprintf(defname, sizeof(defname), "%s-default", data.vendor[i]);
    snprintf(supname, sizeof(defname), "%s-supported", data.vendor[i]);


    /*
     * Here we just pullout the attribute from the preset ...
     */

    if ((defattr = ippFindAttribute(iterator_preset->driver_attrs, defname, IPP_TAG_ZERO)) != NULL)
    {
      ippAttributeString(defattr, defvalue, sizeof(defvalue));
    }
      
    else
      defvalue[0] = '\0';

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, data.vendor[i]));
    snprintf(buffer, sizeof(buffer), "%s-checkbox", data.vendor[i]);

    if(iterator_preset->is_vendor[i])
    {
      papplClientHTMLPrintf(client, "  <input type=\"checkbox\" checked id=\"%s\" >", buffer);
    }
    else
    {
      papplClientHTMLPrintf(client, "  <input type=\"checkbox\"  id=\"%s\" >", buffer);


    }


    if ((supattr = ippFindAttribute(printer->driver_attrs, supname, IPP_TAG_ZERO)) != NULL)
    {

      count = (int)ippGetCount(supattr);

      switch (ippGetValueTag(supattr))
      {
        case IPP_TAG_BOOLEAN :
            if(iterator_preset->is_vendor[i])
              papplClientHTMLPrintf(client, "<input type=\"checkbox\" name=\"%s\"%s>", data.vendor[i], !strcmp(defvalue, "true") ? " checked" : "");
            else 
              papplClientHTMLPrintf(client, "<input type=\"checkbox\" disabled name=\"%s\"%s>", data.vendor[i], !strcmp(defvalue, "true") ? " checked" : "");
            break;

        case IPP_TAG_INTEGER :
            if(iterator_preset->is_vendor[i])
            {
              papplClientHTMLPrintf(client, "<select name=\"%s\">", data.vendor[i]);

            }
            else
            {
              papplClientHTMLPrintf(client, "<select disabled name=\"%s\">", data.vendor[i]);

            }

            for (j = 0; j < count; j ++)
            {
              int val = ippGetInteger(supattr, (int)j);

	      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d</option>", val, val == (int)strtol(defvalue, NULL, 10) ? " selected" : "", val);
            }
            papplClientHTMLPuts(client, "</select>");
            break;

        case IPP_TAG_RANGE :
            {
              int upper, lower = ippGetRange(supattr, 0, &upper);
					// Range
            if(iterator_preset->is_vendor[i])
	            papplClientHTMLPrintf(client, "<input type=\"number\" name=\"%s\" min=\"%d\" max=\"%d\" value=\"%s\">", data.vendor[i], lower, upper, defvalue);
            else
	      papplClientHTMLPrintf(client, "<input type=\"number\" disabled name=\"%s\" min=\"%d\" max=\"%d\" value=\"%s\">", data.vendor[i], lower, upper, defvalue);

	    }
            break;

        case IPP_TAG_KEYWORD :
            if(iterator_preset->is_vendor[i])
              papplClientHTMLPrintf(client, "<select  name=\"%s\">", data.vendor[i]);
            else
              papplClientHTMLPrintf(client, "<select disabled name=\"%s\">", data.vendor[i]);
            for (j = 0; j < count; j ++)
            {
              const char *val = ippGetString(supattr, (int)j, NULL);

	      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", val, !strcmp(val, defvalue) ? " selected" : "", localize_keyword(client, data.vendor[i], val, text, sizeof(text)));
            }
            papplClientHTMLPuts(client, "</select>");
            break;

	default :
	    papplClientHTMLPuts(client, "Unsupported value syntax.");
	    break;
      }
    }
    else
    {
      if(iterator_preset->is_vendor[i])
        papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s\" value=\"%s\">", data.vendor[i], defvalue);

      else
        papplClientHTMLPrintf(client, "<input type=\"text\" disabled name=\"%s\" value=\"%s\">", data.vendor[i], defvalue);
    }
    // papplClientHTMLPrintf(client, "<script> setupCheckboxRadioInteraction(\"%s\", \"%s\");  setupCheckboxSelectInteraction(\"%s\", \"%s\"); </script>", buffer, data.vendor[i],buffer,data.vendor[i]);
    papplClientHTMLPrintf(client, "<script>  setup_Interaction(\"%s\", \"%s\")  </script>", buffer, data.vendor[i]);


    papplClientHTMLPuts(client, "</td></tr>\n");
  }


for (int i = 0; i < sizeof(static_attribute_names) / sizeof(static_attribute_names[0]); i++) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%s-checkbox", static_attribute_names[i]);
        // printf("%s\n", buffer);
        papplClientHTMLPrintf(client, "<script>   console.log(\"%s\");  </script> ",buffer);
        papplClientHTMLPrintf(client, "<script> setup_Interaction(\"%s\", \"%s\"); </script>", buffer, static_attribute_names[i]);
        // papplClientHTMLPrintf(client, "<script> setupCheckboxRadioInteraction(\"%s\", \"%s\");  setupCheckboxSelectInteraction(\"%s\", \"%s\"); </script>", buffer, static_attribute_names[i],buffer, static_attribute_names[i]);
    }

  papplClientHTMLPrintf(client, "<script> document.addEventListener('DOMContentLoaded', function() { console.log('function is working fine'); var form = document.getElementById('form'); if (form) { form.addEventListener('submit', function() { var checkboxes = form.querySelectorAll('input[type=\"checkbox\"]'); checkboxes.forEach(function(checkbox) { checkbox.disabled = true; }); }); } }); </script>");


  _papplRWUnlock(printer);

  papplClientHTMLPrintf(client,
                        "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
                        "            </tbody>\n"
                        "          </table>"
                        "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Save")));

  papplClientHTMLPrinterFooter(client);

  
}

//
// '_papplPrinterPresetCreate' - Show the Preset Create Web Page.
//

void _papplPrinterPresetCreate(

  pappl_client_t *client,
  pappl_printer_t *printer)
{

  // here we fetch data from the driver ....

  int			i, j;		// Looping vars
  pappl_pr_driver_data_t data;		// Driver data
  const char		*keyword;	// Current keyword
  char			text[256];	// Localized text for keyword
  const char		*status = NULL;	// Status message, if any
  bool			show_source = false;
					// Show the media source?
  static const char * const orients[] =	// orientation-requested strings
  {
    _PAPPL_LOC("Portrait"),
    _PAPPL_LOC("Landscape"),
    _PAPPL_LOC("Reverse Landscape"),
    _PAPPL_LOC("Reverse Portrait"),
    _PAPPL_LOC("Auto")
  };
  static const char * const orient_svgs[] =
  {					// orientation-requested images
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='18' font-size='18' fill='currentColor' rotate='0'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='19' font-size='18' fill='currentColor' rotate='-90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='6' font-size='18' fill='currentColor' rotate='90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='7' font-size='18' fill='currentColor' rotate='180'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='5' y='18' font-size='18' fill='currentColor' rotate='0'%3e?%3c/text%3e%3c/svg%3e"
  };

  static const char * const static_attribute_names[] = 
  {
     "\\media",
     "orientation-requested" ,
     "print-color-mode", "sides", "output-bin", 
     "print-quality", "print-darkness", "print-speed",
     "print-content-optimize","print-scaling",
     "print-resolution"
  };

  

  if (!papplClientHTMLAuthorize(client))
    return;

  // get all the driver data over here ...
  papplPrinterGetDriverData(printer, &data);

  if (client->operation == HTTP_STATE_POST)
  {

    int		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    int			num_vendor = 0;	// Number of vendor options
    cups_option_t	*vendor = NULL;	// Vendor options

    if ((num_form = (int)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    // after fetching the data from the web forms .... ( here add those into the driver ... ( saving data ))...
    else
    {
      const char	*value;		// Value of form variable
      char		*end;			// End of value
      int count ;
      pappl_pr_preset_data_t *preset = calloc(1 , sizeof(pappl_pr_preset_data_t));
      preset->driver_attrs = ippNew();
      ipp_t * col = ippNew();

      // define the preset collection over here itself...

      count = cupsArrayGetCount(printer->presets);
      preset->preset_id = count + 1;


      if((value = cupsGetOption("preset_name", num_form , form)) != NULL)
      {
        preset->name = strdup(value);

        ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "preset_name",
                       NULL, strdup(value));

        int preset_iterator , preset_count;
        int flag = 0;

        // below we are checking whether with same name another preset exist or not....
        preset_count = cupsArrayGetCount(printer->presets);
        pappl_pr_preset_data_t * iterator_preset;
        for(preset_iterator = 0;preset_iterator < preset_count; preset_iterator++)
        {
          iterator_preset = cupsArrayGetElement(printer->presets, preset_iterator);
          if(!strcasecmp(iterator_preset->name , preset->name))
          {
            flag = 1;
            break;
          }
        }

          if(flag)
          {
            preset->name = NULL;
            status = _PAPPL_LOC("Preset With the same name already exits.");
          }
          else
          {

            // write the code to save the preset as IPP_T object.....
            
            // create a pointer of ipp_t object to store multiple presets in that ....
            // ipp_t * preset_collection;

            // ipp_attribute_t * final_preset = ippAddCollections(printer->attrs, IPP_TAG_PRINTER,"job-presets-supported", 2, NULL);




            // overe here it's finished ...

            // write other code over here ...
            if ((value = cupsGetOption("orientation-requested", num_form, form)) != NULL)
            {
              preset->orient_default_check = true;
              data.orient_default = (ipp_orient_t)strtol(value, &end, 10);
                ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "orientation-requested-default", NULL,ippEnumString("orientation-requested", (int)(ipp_orient_t)strtol(value, &end, 10)) );

              if (errno == ERANGE || *end || data.orient_default < IPP_ORIENT_PORTRAIT || data.orient_default > IPP_ORIENT_NONE)
                data.orient_default = IPP_ORIENT_PORTRAIT;
            }


            if ((value = cupsGetOption("output-bin", num_form, form)) != NULL)
            {
                ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "output-bin-default", NULL, value );
              preset->bin_default_check = true;
              for (i = 0; i < data.num_bin; i ++)
              {
                if (!strcmp(data.bin[i], value))
                {
                  data.bin_default = i;
                  break;
                }
        }
            }

            if ((value = cupsGetOption("print-color-mode", num_form, form)) != NULL)
            {
                ippAddString(col, IPP_TAG_PRINTER,IPP_TAG_KEYWORD, "print-color-mode-default", NULL,
                             _papplColorModeString(_papplColorModeValue(value)));
              preset->color_default_check = true;
              data.color_default = _papplColorModeValue(value);
            }


            if ((value = cupsGetOption("print-content-optimize", num_form, form)) != NULL)
            {
                ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-content-optimize-default",NULL,
                             _papplContentString(_papplContentValue(value)));
              preset->content_default_check = true;
              data.content_default = _papplContentValue(value);
            }


            if ((value = cupsGetOption("print-darkness", num_form, form)) != NULL)
            {
              ippAddInteger(col,IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-darkness-configured", (int)strtol(value, &end, 10));
              preset->darkness_configured_check = true;
              data.darkness_configured = (int)strtol(value, &end, 10);

              if (errno == ERANGE || *end || data.darkness_configured < 0 || data.darkness_configured > 100)
                data.darkness_configured = 50;
            }

            if ((value = cupsGetOption("print-quality", num_form, form)) != NULL)
            {
              ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-quality-default", NULL, ippEnumString("print-quality", (int)(ipp_quality_t)ippEnumValue("print-quality", value)));
              preset->quality_defualt_check = true;
              data.quality_default = (ipp_quality_t)ippEnumValue("print-quality", value);
            }

            if ((value = cupsGetOption("print-scaling", num_form, form)) != NULL)
            {
              ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-scaling-default",NULL, _papplScalingString(_papplScalingValue(value)));
              preset->scaling_default_check = true;
              data.scaling_default = _papplScalingValue(value);
            }


            if ((value = cupsGetOption("print-speed", num_form, form)) != NULL)
            {
              ippAddInteger(col,IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-speed-default", (int)strtol(value, &end, 10) * 2540);
              preset->speed_defualt_check = true;
              data.speed_default = (int)strtol(value, &end, 10) * 2540;

              if (errno == ERANGE || *end || data.speed_default < 0 || data.speed_default > data.speed_supported[1])
                data.speed_default = 0;
            }

            if ((value = cupsGetOption("sides", num_form, form)) != NULL)
            {
              ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-default", NULL, _papplSidesString(_papplSidesValue(value)));
              preset->sides_default_check = true;
              data.sides_default = _papplSidesValue(value);
            }


            if ((value = cupsGetOption("printer-resolution", num_form, form)) != NULL)
            {
              int x_default;
              int y_default;
              if(sscanf(value, "%dx%ddpi", &x_default, &y_default) == 1)
                y_default = x_default;
              

              ippAddResolution(col, IPP_TAG_PRINTER, "printer-resolution-default", (ipp_res_t)0,x_default, y_default );

              preset->x_default_check = true;
              preset->y_default_check = true;
              if (sscanf(value, "%dx%ddpi", &data.x_default, &data.y_default) == 1)
                data.y_default = data.x_default;
            }

            if ((value = cupsGetOption("media-source", num_form, form)) != NULL)
            { // media ready is left .. finish this is as well...
              preset->media_default_check = true;
              for (i = 0; i < data.num_source; i ++)
        {
          if (!strcmp(value, data.source[i]))
          {
            data.media_default = data.media_ready[i];
            break;
          }
        }
            }
            // save the vendor attributes over here ... ( vendor options ...)
            printf("   \n");
              // _papplRWLockWrite(printer);

            printf("   \n");

            printf("   \n");
          for (i = 0; i < data.num_vendor; i ++)
          {
            char	supattr[128];		// xxx-supported

            snprintf(supattr, sizeof(supattr), "%s-supported", data.vendor[i]);
            if(cupsGetOption(data.vendor[i], num_form, form) == NULL)
            {
              preset->is_vendor[i] = false;
            }
            if ((value = cupsGetOption(data.vendor[i], num_form, form)) != NULL)
            {
              // now you have the value in the form which is there in the vendor option exist in the driver attrs...
              num_vendor = (int)cupsAddOption(data.vendor[i], value, (int)num_vendor, &vendor);
              preset->is_vendor[i] = true;
              ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,data.vendor[i], NULL, value );
            }
          else if (ippFindAttribute(printer->driver_attrs, supattr, IPP_TAG_BOOLEAN))
          {
            num_vendor = (int)cupsAddOption(data.vendor[i], "false", (int)num_vendor, &vendor);
            ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, data.vendor[i], NULL,"false" );
          }
          }
        // printer->config_time = time(NULL);



              // print all the attributes of col ipp_t object...
              // convert the above into ipp_attribute_t object then set these to the ipp_t object...
              // try to traverse the printer col over here to see whether it contains what we craeated or not...
              // write the logic to print all the attributes...
              ipp_attribute_t *attr;

              // for (attr = ippGetFirstAttribute(col); attr; attr = ippGetNextAttribute(col) ){

              //     const char *name = ippGetName(attr);
              //     int count = ippGetCount(attr);
              //     ipp_tag_t value_tag = ippGetValueTag(attr);

              //     printf("Yaar read krliya finally --name hai ab---> %s\n", name);
              //     printf("the value of count associated with that is ---> %d\n", count);

              //     for (int i = 0; i < count; i++) {
              //         if (value_tag == IPP_TAG_TEXT) {
              //             const char *value = ippGetString(attr, i, NULL);
              //             printf("  Value %d: %s\n", i, value);
              //         } else if (value_tag == IPP_TAG_INTEGER) {
              //             int value = ippGetInteger(attr, i);
              //             printf("  Value %d: %d\n", i, value);
              //         } else if (value_tag == IPP_TAG_BOOLEAN) {
              //             bool value = ippGetBoolean(attr, i);
              //             printf("  Value %d: %d\n", i, value);
              //         } else if (value_tag == IPP_TAG_STRING) {

              //             char *value = ippGetString(attr, i, NULL);
              //             printf("  Value %d: %s\n", i, value);
              //         } else if (value_tag == IPP_TAG_ENUM) {
              //             // here we print the enum values ...
              //             int value = ippGetInteger(attr, i);
              //             printf("  Value %d: %d\n", i, value);

              //        }
              //         else if(value_tag == IPP_TAG_KEYWORD)
              //         {
              //             const char *value = ippGetString(attr, i, NULL);
              //             printf("  Value %d: %s\n", i, value);
              //         }
              //     }
              // }

        // logic to store preset in printer->attrs...
        



            papplPrinterSetPresetFromDriver(printer, &data, preset, num_vendor , vendor );

  //           // write your own logic to save preset in the printer...
              _papplRWLockWrite(printer);
              // finally the rock is cookin ...
              ipp_attribute_t *presets;
              bool checker = false;
              // presets = ippFindAttribute(printer->attrs, "job-presets-supported", IPP_TAG_PRINTER);
              for(presets = ippGetFirstAttribute(printer->attrs); presets; presets = ippGetNextAttribute(printer->attrs))
              {
                if(!strcasecmp(ippGetName(presets), "job-presets-supported"))
                {
                  checker = true;
                  break;
                }
              }
                
              if(checker)
              {
                printf("job-presets-supported  is  there in the printer->attrs right now \n");

                // so you have presets in the printer->attrs ...
                int preset_count = ippGetCount(presets);
                ipp_t *preset_array[preset_count+1];
                for(int iter = 0;iter<preset_count;iter++)
                {
                  ipp_attribute_t *curr_preset = ippGetCollection(presets, iter);
                  preset_array[iter] = curr_preset;
                }
                // add the new preset over here...
                preset_array[preset_count] = col;

                ipp_attribute_t * check_preset;

                ippDeleteAttribute(printer->attrs, presets);
                ippAddCollections(printer->attrs, IPP_TAG_PRINTER, "job-presets-supported", preset_count+1, preset_array);

              }
              else
              {
                printf("job-presets-supported  is not there in the printer->attrs right now \n");
                // here for now I am writing the logic to add job-presets-supported in printer->attrs
                // instead this should be perform in printer.c while adding the presets from the file.
                // after creating the printer object...
                ipp_t *preset_array[1];
                preset_array[0] = col;
                ippAddCollections(printer->attrs, IPP_TAG_PRINTER, "job-presets-supported", 1, preset_array);
              }


        _papplRWUnlock(printer);

        _papplSystemConfigChanged(printer->system);


              if (papplPrinterAddPresetCreate( printer , preset))
                    status = _PAPPL_LOC("Changes saved.");
                  else
                    status = _PAPPL_LOC("Bad preset values.");
            

            cupsFreeOptions((int)num_vendor, vendor);
    }
          }

        }



    cupsFreeOptions(num_form, form);
  }

  /*
   * If you don't wanna do some changes then this one ...
   */

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Let's create a Preset for you ...\n"), 0, NULL, NULL);


  if (status)
  {
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));
  }

  // here we are starting the web forms ... to get data ....
  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  
   papplClientHTMLPrintf(client,
    " <tr> <th><label for=\"printer_name\">%s:</label><br>\n </th>"
    "<td> <input type=\"text\" name=\"preset_name\" placeholder=\"%s\" required><br> </td></tr>\n",
    papplClientGetLocString(client, _PAPPL_LOC("Name")),
    papplClientGetLocString(client, _PAPPL_LOC("Name of Preset")));
  



  // media-col-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "media"));
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"\\media-checkbox\" >");

  if (data.num_source > 1)
  {
    papplClientHTMLPuts(client, "<select name=\"media-source\">");

    for (i = 0; i < data.num_source; i ++)
    {
      // See if any two sources have the same size...
      for (j = i + 1; j < data.num_source; j ++)
      {
        if (data.media_ready[i].size_width > 0 && data.media_ready[i].size_width == data.media_ready[j].size_width && data.media_ready[i].size_length == data.media_ready[j].size_length)
        {
          show_source = true;
          break;
        }
      }
    }

    for (i = 0; i < data.num_source; i ++)
    {
      keyword = data.source[i];

      if (strcmp(keyword, "manual"))
      {
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, !strcmp(keyword, data.media_default.source) ? " selected" : "", localize_media(client, data.media_ready + i, show_source, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</select>");
  }
  else
    papplClientHTMLEscape(client, localize_media(client, data.media_ready, false, text, sizeof(text)), 0);

  papplClientHTMLPrintf(client, " <a class=\"btn\" disabled href=\"%s/media\">%s</a></td></tr>\n", printer->uriname, papplClientGetLocString(client, _PAPPL_LOC("Configure Media")));

  // orientation-requested-default

  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "orientation-requested"));
  papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"orientation-requested-checkbox\" >");
  for (i = IPP_ORIENT_PORTRAIT; i <= IPP_ORIENT_NONE; i ++)
  {
    papplClientHTMLPrintf(client, "<label class=\"image\"><input type=\"radio\" name=\"orientation-requested\" disabled value=\"%d\"%s> <img src=\"data:image/svg+xml,%s\" alt=\"%s\"></label> ", i, data.orient_default == (ipp_orient_t)i ? " checked" : "", orient_svgs[i - IPP_ORIENT_PORTRAIT], orients[i - IPP_ORIENT_PORTRAIT]);
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // print-color-mode-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-color-mode"));
  papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-color-mode-checkbox\" >");

  if (data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME) || data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_AUTO_MONOCHROME))
  {
    papplClientHTMLPuts(client, "B&amp;W");
  }
  else
  {
    for (i = PAPPL_COLOR_MODE_AUTO; i <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; i *= 2)
    {
      if ((data.color_supported & (pappl_color_mode_t)i) && i != PAPPL_COLOR_MODE_AUTO_MONOCHROME)
      {
	keyword = _papplColorModeString((pappl_color_mode_t)i);
	papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-color-mode\" disabled value=\"%s\"%s> %s</label> ", keyword, (pappl_color_mode_t)i == data.color_default ? " checked" : "", localize_keyword(client, "print-color-mode", keyword, text, sizeof(text)));
      }
    }
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  if (data.sides_supported && data.sides_supported != PAPPL_SIDES_ONE_SIDED)
  {
    // sides-default
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "sides"));
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"sides-checkbox\" >");
    for (i = PAPPL_SIDES_ONE_SIDED; i <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; i *= 2)
    {
      if (data.sides_supported & (pappl_sides_t)i)
      {
	keyword = _papplSidesString((pappl_sides_t)i);
	papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"sides\" disabled value=\"%s\"%s> %s</label> ", keyword, (pappl_sides_t)i == data.sides_default ? " checked" : "", localize_keyword(client, "sides", keyword, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // output-bin-default
  if (data.num_bin > 0)
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "output-bin"));
    if (data.num_bin > 1)
    {
      papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"output-bin-checkbox\" >");
      papplClientHTMLPuts(client, "<select name=\"output-bin\" disabled>");
      for (i = 0; i < data.num_bin; i ++)
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", data.bin[i], i == data.bin_default ? " selected" : "", localize_keyword(client, "output-bin", data.bin[i], text, sizeof(text)));
      papplClientHTMLPuts(client, "</select>");
    }
    else
    {
      papplClientHTMLPrintf(client, "%s", localize_keyword(client, "output-bin", data.bin[data.bin_default], text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // print-quality-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-quality"));
  papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-quality-checkbox\" >");
  for (i = IPP_QUALITY_DRAFT; i <= IPP_QUALITY_HIGH; i ++)
  {
    keyword = ippEnumString("print-quality", i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-quality\" disabled value=\"%s\"%s> %s</label> ", keyword, (ipp_quality_t)i == data.quality_default ? " checked" : "", localize_keyword(client, "print-quality", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-darkness-configured
  if (data.darkness_supported)
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-darkness"));
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-darkness-checkbox\" >");
    papplClientHTMLPrintf(client, "<select disabled name=\"print-darkness\">");
    for (i = 0; i < data.darkness_supported; i ++)
    {
      int percent = 100 * i / (data.darkness_supported - 1);
					// Percent darkness

      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d%%</option>", percent, percent == data.darkness_configured ? " selected" : "", percent);
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-speed-default
  if (data.speed_supported[1])
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th>", papplClientGetLocString(client, "print-speed"), papplClientGetLocString(client, _PAPPL_LOC("Auto")));
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-speed-checkbox\" >");
    for (i = data.speed_supported[0]; i <= data.speed_supported[1]; i += 2540)
    {
      if (i > 0)
      {
        papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), i > 2540 ? _PAPPL_LOC("%d inches/sec") : _PAPPL_LOC("%d inch/sec"), i / 2540);
	papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%s</option>", i / 2540, i == data.speed_default ? " selected" : "", text);
      }
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-content-optimize-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th> <td>", papplClientGetLocString(client, "print-content-optimize"));
  papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-content-optimize-checkbox\" >");
  papplClientHTMLPrintf(client, "<select disabled name=\"print-content-optimize\">");

  for (i = PAPPL_CONTENT_AUTO; i <= PAPPL_CONTENT_TEXT_AND_GRAPHIC; i *= 2)
  {
    keyword = _papplContentString((pappl_content_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_content_t)i == data.content_default ? " selected" : "", localize_keyword(client, "print-content-optimize", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-scaling-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-scaling"));
  papplClientHTMLPrintf(client, "  <input type=\"checkbox\"  id=\"print-scaling-checkbox\" >");
  papplClientHTMLPrintf(client, "<select disabled name=\"print-scaling\">");

  for (i = PAPPL_SCALING_AUTO; i <= PAPPL_SCALING_NONE; i *= 2)
  {
    keyword = _papplScalingString((pappl_scaling_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_scaling_t)i == data.scaling_default ? " selected" : "", localize_keyword(client, "print-scaling", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // printer-resolution-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "printer-resolution"));
  papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"printer-resolution-checkbox\" >");

  if (data.num_resolution == 1)
  {
    if (data.x_resolution[0] != data.y_resolution[0])
      papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%dx%ddpi")), data.x_resolution[0], data.y_resolution[0]);
    else
      papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%ddpi")), data.x_resolution[0]);
  }
  else
  {
    papplClientHTMLPuts(client, "<select disabled name=\"printer-resolution\">");
    for (i = 0; i < data.num_resolution; i ++)
    {
      if (data.x_resolution[i] != data.y_resolution[i])
        papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%dx%ddpi"), data.x_resolution[i], data.y_resolution[i]);
      else
	papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%ddpi"), data.x_resolution[i]);

      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", text, (data.x_default == data.x_resolution[i] && data.y_default == data.y_resolution[i]) ? " selected" : "", text);
    }
    papplClientHTMLPuts(client, "</select>");
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // papplClientHTMLPrintf(client , "<script> function setupCheckboxSelectInteraction(checkboxId, selectName) { var checkbox = document.getElementById(checkboxId); var selectElement = document.querySelector('select[name=\"' + selectName + '\"]'); if (checkbox && selectElement) { checkbox.addEventListener('change', function() { selectElement.disabled = !checkbox.checked; }); } } function setupCheckboxRadioInteraction(checkboxId, radioName) { var checkbox = document.getElementById(checkboxId); var radioButtons = document.querySelectorAll('input[type=\"radio\"][name=\"' + radioName + '\"]'); if (checkbox && radioButtons.length > 0) { checkbox.addEventListener('change', function() { radioButtons.forEach(function(radioButton) { radioButton.disabled = !checkbox.checked; }); }); } } </script>");

    // papplClientHTMLPrintf(client, "<script> function setup_Interaction(checkboxId, inputName) { var types = [\"select\", \"radio\", \"text\", \"number\", \"checkbox\"]; var checkbox = document.getElementById(checkboxId); types.forEach(function(type)) { var inputs = document.querySelectorAll('input[type=\"' + type + '\"][name=\"' + inputName + '\"]'); if (checkbox && inputs.length > 0) { checkbox.addEventListener('change', function() { inputs.forEach(function(input) { input.disabled = !checkbox.checked; }); }); } } } </script>");
  papplClientHTMLPrintf(client, "<script> function setup_Interaction(checkboxId, inputName) { var types = [\"select\", \"radio\", \"text\", \"number\", \"checkbox\"]; var checkbox = document.getElementById(checkboxId); types.forEach(function (type) { var createdPattern; switch (type) { case \"select\": createdPattern = 'select[name=\"' + inputName + '\"]'; break; case \"radio\": createdPattern = 'input[type=\"radio\"][name=\"' + inputName + '\"]'; break; case \"text\": created_pattern = 'input[type=\"text\"][name=\"' + inputName + '\"]'; case \"number\": created_pattern = 'input[type=\"number\"][name=\"' + inputName + '\"]'; case \"checkbox\": created_pattern = 'input[type=\"checkbox\"][name=\"' + inputName + '\"]'; } console.log(createdPattern); var inputs = document.querySelectorAll(createdPattern); if (checkbox && inputs.length > 0) { checkbox.addEventListener(\"change\", function () { inputs.forEach(function (input) { input.disabled = !checkbox.checked; }); }); } }); } </script>");
  
  // Vendor options
  _papplRWLockRead(printer);

  for (i = 0; i < data.num_vendor; i ++)
  {
    char	defname[128],		// xxx-default name
		defvalue[1024],		// xxx-default value
		supname[128];		// xxx-supported name
    ipp_attribute_t *defattr,		// xxx-default attribute
	      	*supattr;		// xxx-supported attribute
    int		count;			// Number of values
    char buffer[1024];

    snprintf(defname, sizeof(defname), "%s-default", data.vendor[i]);
    snprintf(supname, sizeof(defname), "%s-supported", data.vendor[i]);

    if ((defattr = ippFindAttribute(printer->driver_attrs, defname, IPP_TAG_ZERO)) != NULL)
      ippAttributeString(defattr, defvalue, sizeof(defvalue));
    else
      defvalue[0] = '\0';

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, data.vendor[i]));
    // create the checkbox id over here using the buffer that you have created ..
    snprintf(buffer, sizeof(buffer), "%s-checkbox", data.vendor[i]);
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\"  id=\"%s\" >", buffer);


    printf("%s\n", data.vendor[i]);
    if ((supattr = ippFindAttribute(printer->driver_attrs, supname, IPP_TAG_ZERO)) != NULL)
    {
      count = (int)ippGetCount(supattr);

      switch (ippGetValueTag(supattr))
      {
        case IPP_TAG_BOOLEAN :
        // add the checkbox over here ...
            papplClientHTMLPrintf(client, "<input disabled type=\"checkbox\" name=\"%s\"%s>", data.vendor[i], !strcmp(defvalue, "true") ? " checked" : "");
            break;

        case IPP_TAG_INTEGER :
        // add the checkbox over here ...
            papplClientHTMLPrintf(client, "<select disabled name=\"%s\">", data.vendor[i]);
            for (j = 0; j < count; j ++)
            {
              int val = ippGetInteger(supattr, (int)j);

	      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d</option>", val, val == (int)strtol(defvalue, NULL, 10) ? " selected" : "", val);
            }
            papplClientHTMLPuts(client, "</select>");
            break;

        case IPP_TAG_RANGE :
            {
              int upper, lower = ippGetRange(supattr, 0, &upper);
					// Range
        // add the checkbox over here ...

	      papplClientHTMLPrintf(client, "<input disabled type=\"number\" name=\"%s\" min=\"%d\" max=\"%d\" value=\"%s\">", data.vendor[i], lower, upper, defvalue);
	    }
            break;

        case IPP_TAG_KEYWORD :
        // add the checkbox over here ...

            papplClientHTMLPrintf(client, "<select disabled name=\"%s\">", data.vendor[i]);
            for (j = 0; j < count; j ++)
            {
              const char *val = ippGetString(supattr, (int)j, NULL);

	      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", val, !strcmp(val, defvalue) ? " selected" : "", localize_keyword(client, data.vendor[i], val, text, sizeof(text)));
            }
            papplClientHTMLPuts(client, "</select>");
            break;

	default :
	    papplClientHTMLPuts(client, "Unsupported value syntax.");
	    break;
      }
    }
    else
    {
      // Text option
      // add the checkbox over here ... also once the checkbox is added and after that options kind of that
      // are added ... then add the code to run javascript ...
      /// make sure to add the logic to do that for number...
      papplClientHTMLPrintf(client, "<input disabled type=\"text\" name=\"%s\" value=\"%s\">", data.vendor[i], defvalue);
    }
    // papplClientHTMLPrintf(client, "<script> setupCheckboxRadioInteraction(\"%s\", \"%s\");  setupCheckboxSelectInteraction(\"%s\", \"%s\"); </script>", buffer, data.vendor[i],buffer,data.vendor[i]);
    // write the code for interaction over here...
    papplClientHTMLPrintf(client, "<script> setup_Interaction(\"%s\", \"%s\");  </script>", buffer, data.vendor[i]);


    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  _papplRWUnlock(printer);

  // papplClientHTMLPrintf(client , "<script>\n"
  //                       "function setupCheckboxSelectInteraction(checkboxId, selectName) {\n"
  //                       "  var checkbox = document.getElementById(checkboxId);\n"
  //                       "var selectElement = document.querySelector('select[name=\"' + selectName + '\"]');\n"
  //                       "checkbox.addEventListener('change', function() { selectElement.disabled = !checkbox.checked; }); }\n"
  //                       "    function setupCheckboxRadioInteraction(checkboxId, radioName) {\n"
  //                       "      var checkbox = document.getElementById(checkboxId);\n"
  //                       "var radioButtons = document.querySelectorAll('input[type=\"radio\"][name=\"' + radioName + '\"]');\n"
  //                       "checkbox.addEventListener('change', function() {\n"
  //                       "radioButtons.forEach(function(radioButton) { radioButton.disabled = !checkbox.checked; }); }); }\n"
  //                       "</script>"
  //                       );



  // papplClientHTMLPrintf(client, "<script> function setupCheckboxSelectInteraction(checkboxId, selectName) { var checkbox = document.getElementById(checkboxId); var selectElement = document.querySelector('select[name=\"' + selectName + '\"]'); checkbox.addEventListener('change', function() { selectElement.disabled = !checkbox.checked; }); } function setupCheckboxRadioInteraction(checkboxId, radioName) { var checkbox = document.getElementById(checkboxId); var radioButtons = document.querySelectorAll('input[type=\"radio\"][name=\"' + radioName + '\"]'); checkbox.addEventListener('change', function() { radioButtons.forEach(function(radioButton) { radioButton.disabled = !checkbox.checked; }); }); } </script>");
// running for loop to add checkbox logic using javascript functions ...
for (int i = 0; i < sizeof(static_attribute_names) / sizeof(static_attribute_names[0]); i++) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%s-checkbox", static_attribute_names[i]);
        // printf("%s\n", buffer);
        // papplClientHTMLPrintf(client, "<script>   console.log(\"%s\");  </script> ",buffer);
        papplClientHTMLPrintf(client, "<script> setup_Interaction(\"%s\", \"%s\");  </script>", buffer, static_attribute_names[i]);
    }


  // the below is used to remove checkbox data from the 
  papplClientHTMLPrintf(client, "<script> document.addEventListener('DOMContentLoaded', function() { console.log('function is working fine'); var form = document.getElementById('form'); if (form) { form.addEventListener('submit', function() { var checkboxes = form.querySelectorAll('input[type=\"checkbox\"]'); checkboxes.forEach(function(checkbox) { checkbox.disabled = true; }); }); } }); </script>");

  papplClientHTMLPrintf(client,
                        "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
                        "            </tbody>\n"
                        "          </table>"
                        "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Save")));

  papplClientHTMLPrinterFooter(client);


}

// The below function will return an array of ipp_t objects ... 
// after updating the new preset in the array ...
ipp_t** funct(ipp_attribute_t* presets, ipp_t *new_preset, const char *preset_name)
{
  printf("T TTTT --> %s\n", preset_name );
  // what is the task ---> 1. preset name is the one which get's updated ... so take the index in the collection where ipp_t is stored...
  ipp_attribute_t * iterator;

  int preset_count = ippGetCount(presets);
  // write the logic to add the new preset into the above and return the array ...
  // ipp_t* preset_array[preset_count+1];
  ipp_t ** preset_array = malloc(preset_count * sizeof(ipp_t*));
  int storeindex = -1;
  // iterate over all the presets over here ... preset_counct ( count of the preset )
  for(int iter = 0;iter<preset_count;iter++)
  {
    ipp_attribute_t *curr_preset = ippGetCollection(presets, iter);
    char *name;
    char *value;
    // you have the collection ... now traverse and search for it's name ... over here ...
    for(iterator = ippGetFirstAttribute(curr_preset); iterator ; iterator = ippGetNextAttribute(curr_preset))
    {
      // printf("the ippgetname --->%s\n", ippGetName(iterator));
      name = ippGetName(iterator);
      value = ippGetString(iterator, 0, NULL);
      printf("THE NAME --> %s and value --> %s\n", name, value);
      if(!strcasecmp(name, "preset_name"))
      {
        if(!strcasecmp(value, preset_name))
        {
          storeindex = iter;
          break;
        }
      }
    }
  }
  preset_array[storeindex] = new_preset;

  for(int iter = 0;iter< preset_count;iter++)
  {
    if(iter != storeindex)
    {
      ipp_attribute_t *curr_preset = ippGetCollection(presets, iter);
      preset_array[iter] = curr_preset;
    }
  }

        //   for(int xs = 0;xs< preset_count; xs++)
        // {
        //   // now grab a preset ...
        //   ipp_t * ssPR = preset_array[xs];
        //   ipp_attribute_t * again = ippGetFirstAttribute(ssPR);
        //   printf("calling from function with name ---- > %s and value --> %s\n", ippGetName(again), ippGetString(again, 0, NULL));
        // }


  return preset_array;
}

//
// '_papplPrinterPresetEdit' - Show the Preset Edit Web Page.
//

void _papplPrinterPresetEdit(
    pappl_client_t  *client,		// I - Client
    resource_data_t *resource_data
    )		
{
  int			i, j;		// Looping vars
        cups_option_t * check_form = NULL;
        ipp_attribute_t * presets;
        int len;
        int preset_counter;
        char *oht;
  pappl_pr_driver_data_t data;		// Driver data
  const char		*keyword;	// Current keyword
  char			text[256];	// Localized text for keyword
  const char		*status = NULL;	// Status message, if any
  bool			show_source = false;
					// Show the media source?
  static const char * const orients[] =	// orientation-requested strings
  {
    _PAPPL_LOC("Portrait"),
    _PAPPL_LOC("Landscape"),
    _PAPPL_LOC("Reverse Landscape"),
    _PAPPL_LOC("Reverse Portrait"),
    _PAPPL_LOC("Auto")
  };
  static const char * const orient_svgs[] =
  {					// orientation-requested images
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='18' font-size='18' fill='currentColor' rotate='0'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='19' font-size='18' fill='currentColor' rotate='-90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='6' font-size='18' fill='currentColor' rotate='90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='7' font-size='18' fill='currentColor' rotate='180'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='5' y='18' font-size='18' fill='currentColor' rotate='0'%3e?%3c/text%3e%3c/svg%3e"
  };


  static const char * const static_attribute_names[] = 
  {
     "orientation-requested" ,
     "print-color-mode", "sides", "output-bin", 
     "print-quality", "print-darkness", "print-speed",
     "print-content-optimize","print-scaling",
     "print-resolution"
  };



  // checking whether we have authorized client or not ....

  if (!papplClientHTMLAuthorize(client))
    return;

  pappl_printer_t * printer = resource_data->printer;
  const char *preset_name = resource_data->preset_name;


  // get all the driver data over here ...
  papplPrinterGetDriverData(printer, &data);

  // write the logic to grab particular preset ...
  int preset_iterator , preset_count;
  preset_count = cupsArrayGetCount(printer->presets);
  pappl_pr_preset_data_t * iterator_preset;
  for(preset_iterator = 0;preset_iterator < preset_count; preset_iterator++)
  {
    iterator_preset = cupsArrayGetElement(printer->presets, preset_iterator);
    if(!strcasecmp(iterator_preset->name , preset_name))
      break;
  }

  
  if (client->operation == HTTP_STATE_POST)
  {

    // instance variables ...
    int		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    int			num_vendor = 0;	// Number of vendor options
    cups_option_t	*vendor = NULL;	// Vendor options
    char *pre_name_from_form;

    if ((num_form = (int)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }

      // after fetching the data from the web forms .... ( here add those into the driver ... ( saving data ))  
    else
    {

      // char something[1024];

      // char buf_me[8096];
      // // memset(iterator_preset->is_vendor, false , sizeof(iterator_preset->is_vendor));
      // for (size_t i = 0; i < num_form; i++) {
      //   strcat(buf_me, form[i].name);
      //   strcat(buf_me, "----");
      //   strcat(buf_me, form[i].value);
      //   strcat(buf_me, "\n");
      //   printf("Option Name in the web default page  : %s\n", form[i].name);
      //   printf("Option Value in the web defualt page : %s\n", form[i].value);
      //   printf("\n");
      // }

      const char	*value;		// Value of form variable
      char		*end;			// End of value
      ipp_t * col = ippNew();

      // set the the data into preset .... ( The standard one ...)
      if((value = cupsGetOption("old_preset_name", num_form, form)) != NULL)
      {
        pre_name_from_form = strdup(value);

      }

      if((value = cupsGetOption("old_preset_count", num_form, form)) != NULL)
      {
        preset_counter = (int)strtol(value, &end, 10);
      }

      if((value = cupsGetOption("preset_name", num_form , form)) != NULL)
      {
        iterator_preset->name = strdup(value);
        ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "preset_name", NULL, strdup(value));
      }
      

      if ((value = cupsGetOption("orientation-requested", num_form, form)) != NULL)
      {
                ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "orientation-requested-default", NULL,ippEnumString("orientation-requested", (int)(ipp_orient_t)strtol(value, &end, 10)) );

        iterator_preset->orient_default_check = true;
        iterator_preset->orient_default = (ipp_orient_t)strtol(value, &end, 10);

        if (errno == ERANGE || *end || iterator_preset->orient_default < IPP_ORIENT_PORTRAIT || iterator_preset->orient_default > IPP_ORIENT_NONE)
          iterator_preset->orient_default = IPP_ORIENT_PORTRAIT;
      }

      if ((value = cupsGetOption("output-bin", num_form, form)) != NULL)
      {
                ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "output-bin-default", NULL, value );

        iterator_preset->bin_default_check = true;
        for (i = 0; i < iterator_preset->num_bin; i ++)
        {
          if (!strcmp(iterator_preset->bin[i], value))
          {
            iterator_preset->bin_default = i;
            break;
          }
	}
      }

      if ((value = cupsGetOption("print-color-mode", num_form, form)) != NULL)
      {
                        ippAddString(col, IPP_TAG_PRINTER,IPP_TAG_KEYWORD, "print-color-mode-default", NULL,
                             _papplColorModeString(_papplColorModeValue(value)));
        iterator_preset->color_default_check = true;
        iterator_preset->color_default = _papplColorModeValue(value);
      }

      if ((value = cupsGetOption("print-content-optimize", num_form, form)) != NULL)
      {
                        ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-content-optimize-default",NULL,
                             _papplContentString(_papplContentValue(value)));
        iterator_preset->content_default_check = true;
        iterator_preset->content_default = _papplContentValue(value);
      }

      if ((value = cupsGetOption("print-darkness", num_form, form)) != NULL)
      {
              ippAddInteger(col,IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-darkness-configured", (int)strtol(value, &end, 10));

        iterator_preset->darkness_configured_check = true;
        iterator_preset->darkness_configured = (int)strtol(value, &end, 10);

        if (errno == ERANGE || *end || iterator_preset->darkness_configured < 0 || iterator_preset->darkness_configured > 100)
          iterator_preset->darkness_configured = 50;
      }

      if ((value = cupsGetOption("print-quality", num_form, form)) != NULL)
      {
              ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-quality-default", NULL, ippEnumString("print-quality", (int)(ipp_quality_t)ippEnumValue("print-quality", value)));

        iterator_preset->quality_defualt_check =true;
        iterator_preset->quality_default = (ipp_quality_t)ippEnumValue("print-quality", value);

      }

      if ((value = cupsGetOption("print-scaling", num_form, form)) != NULL)
      {
              ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-scaling-default",NULL, _papplScalingString(_papplScalingValue(value)));

        iterator_preset->scaling_default_check = true;
        iterator_preset->scaling_default = _papplScalingValue(value);

      }

      if ((value = cupsGetOption("print-speed", num_form, form)) != NULL)
      {
              ippAddInteger(col,IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "print-speed-default", (int)strtol(value, &end, 10) * 2540);

        iterator_preset->speed_defualt_check = true;
        iterator_preset->speed_default = (int)strtol(value, &end, 10) * 2540;

        if (errno == ERANGE || *end || iterator_preset->speed_default < 0 || iterator_preset->speed_default > iterator_preset->speed_supported[1])
          iterator_preset->speed_default = 0;
      }

      if ((value = cupsGetOption("sides", num_form, form)) != NULL)
      {
              ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "sides-default", NULL, _papplSidesString(_papplSidesValue(value)));

        iterator_preset->sides_default_check = true;
        iterator_preset->sides_default = _papplSidesValue(value);

      }

      if ((value = cupsGetOption("printer-resolution", num_form, form)) != NULL)
      {
                      int x_default;
              int y_default;
              if(sscanf(value, "%dx%ddpi", &x_default, &y_default) == 1)
                y_default = x_default;
              

              ippAddResolution(col, IPP_TAG_PRINTER, "printer-resolution-default", (ipp_res_t)0,x_default, y_default );

        iterator_preset->x_default_check = true;
        iterator_preset->y_default_check = true;
        if (sscanf(value, "%dx%ddpi", &iterator_preset->x_default, &iterator_preset->y_default) == 1)
          iterator_preset->y_default = iterator_preset->x_default;
      }

      if ((value = cupsGetOption("media-source", num_form, form)) != NULL)
      {
        iterator_preset->media_default_check = true;
        for (i = 0; i < iterator_preset->num_source; i ++)
	{
	  if (!strcmp(value, iterator_preset->source[i]))
	  {
	    iterator_preset->media_default = iterator_preset->media_ready[i];
	    break;
	  }
	}
      }

      // save the vendor attributes over here ... ( vendor options ...)


      for (i = 0; i < data.num_vendor; i ++)
      {
        char	supattr[128];		// xxx-supported

        snprintf(supattr, sizeof(supattr), "%s-supported", data.vendor[i]);
        // printf("The iterator can make supattr and it's value is ---- %s\n",supattr );
        if(cupsGetOption(data.vendor[i], num_form, form) == NULL)
        {
          iterator_preset->is_vendor[i] = false;
        }

        else if((value = cupsGetOption(data.vendor[i], num_form, form)) != NULL)
        {          
          iterator_preset->is_vendor[i] = true;
          printf("The if value that get changed in form is ---- %s --- %s\n", value , data.vendor[i]);
          num_vendor = (int)cupsAddOption(data.vendor[i], value, (int)num_vendor, &vendor);
              ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,data.vendor[i], NULL, value );

        }


        else if (ippFindAttribute(printer->driver_attrs, supattr, IPP_TAG_BOOLEAN))
        {
            ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, data.vendor[i], NULL,"false" );

          printf("The else value that get changed in form is ---- %s --- %s\n", value , data.vendor[i]);
          num_vendor = (int)cupsAddOption(data.vendor[i], "false", (int)num_vendor, &vendor);
        }
          
      }


          
                for(presets = ippGetFirstAttribute(resource_data->printer->attrs); presets; presets = ippGetNextAttribute(resource_data->printer->attrs))
          if(!strcasecmp(ippGetName(presets), "job-presets-supported"))
            break;


        // ipp_t * preset_array[ippGetCount(presets)];
        ipp_t ** preset_arrayp = funct(presets, col, pre_name_from_form);

                _papplRWLockWrite(resource_data->printer);
        ippDeleteAttribute(printer->attrs, presets);
        int zi = sizeof(preset_arrayp);
        printf("THe size of presets that we recieve --> %d\n ", preset_counter);

        // check whether presets in the array are updated properly ...

        // ipp_t *current_element = preset_arrayp; // Point to the beginning of the array

// Traverse the array until reaching the end

         
        // for(int xs = 0;xs< preset_counter; xs++)
        // {printf("Rhe \n");
        //   // now grab a preset ...
        //   ipp_t * ssPR = preset_arrayp[0];
        //   printf("Rhe \n");

        //   ipp_attribute_t * again = ippGetFirstAttribute(ssPR);
        //   printf("Rhe \n");

        //   printf("THE ankit i---- > %s\n", ippGetName(again));
        //   printf("Rhe \n");
        // }





        ippAddCollections(resource_data->printer->attrs, IPP_TAG_PRINTER, "job-presets-supported", preset_counter, preset_arrayp);

        _papplRWUnlock(resource_data->printer);
        _papplSystemConfigChanged(printer->system);

      // printf("working or not\n");

      if (papplPrinterSetPresetsVendor(printer, iterator_preset, num_vendor, vendor))
        status = _PAPPL_LOC("Changes saved.");
      else
        status = _PAPPL_LOC("Bad preset values.");

      cupsFreeOptions((int)num_vendor, vendor);
    }

    cupsFreeOptions(num_form, form);
  }
  else
  {
      // cups_option_t * check_form = NULL;
      len = (cups_len_t)papplClientGetForm(client, &check_form);
      // oht = cupsGetOption("name", len, check_form);
      if(check_form != NULL)
      {
        for(presets = ippGetFirstAttribute(resource_data->printer->attrs); presets; presets = ippGetNextAttribute(resource_data->printer->attrs))
          if(!strcasecmp(ippGetName(presets), "job-presets-supported"))
            break;
        // preset_counter = ippGetCount(presets);
      }
  }

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Let's Edit your preset over here..."), 0, NULL, NULL);



  if (status)
  {
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));
  }

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

   char		printer_name[128] = "";	// Printer Name


  papplClientHTMLPrintf(client,
    " <tr> <th><label for=\"printer_name\">%s:</label><br>\n </th>"
    "<td> <input type=\"text\" name=\"preset_name\" placeholder=\"%s\" value=\"%s\" required><br> </td></tr>\n",
    papplClientGetLocString(client, _PAPPL_LOC("Name")),
    papplClientGetLocString(client, _PAPPL_LOC("Name of Preset")), iterator_preset->name);


    // media-col-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "media"));

  if (data.num_source > 1)
  {
    papplClientHTMLPuts(client, "<select name=\"media-source\">");

    for (i = 0; i < data.num_source; i ++)
    {
      // See if any two sources have the same size...
      for (j = i + 1; j < data.num_source; j ++)
      {
        if (iterator_preset->media_ready[i].size_width > 0 && iterator_preset->media_ready[i].size_width == iterator_preset->media_ready[j].size_width && iterator_preset->media_ready[i].size_length == iterator_preset->media_ready[j].size_length)
        {
          show_source = true;
          break;
        }
      }
    }

    for (i = 0; i < data.num_source; i ++)
    {
      keyword = data.source[i];

      if (strcmp(keyword, "manual"))
      {
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, !strcmp(keyword, iterator_preset->media_default.source) ? " selected" : "", localize_media(client, iterator_preset->media_ready + i, show_source, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</select>");
  }
  else
    papplClientHTMLEscape(client, localize_media(client, iterator_preset->media_ready, false, text, sizeof(text)), 0);

  papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s/media\">%s</a></td></tr>\n", printer->uriname, papplClientGetLocString(client, _PAPPL_LOC("Configure Media")));

  // add old preset-name
  papplClientHTMLPrintf(client, " <input type=\"hidden\" name=\"old_preset_name\" value=%s> ",iterator_preset->name);

  // add preset count over here ...
    papplClientHTMLPrintf(client, " <input type=\"hidden\" name=\"old_preset_count\" value=%d> ", preset_count);


  // orientation-requested-default

  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "orientation-requested"));
  if(iterator_preset->orient_default_check)
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"orientation-requested-checkbox\" checked >");
    for (i = IPP_ORIENT_PORTRAIT; i <= IPP_ORIENT_NONE; i ++)
    {
      papplClientHTMLPrintf(client, "<label class=\"image\"><input type=\"radio\" name=\"orientation-requested\" value=\"%d\"%s> <img src=\"data:image/svg+xml,%s\" alt=\"%s\"></label> ", i, iterator_preset->orient_default == (ipp_orient_t)i ? " checked" : "", orient_svgs[i - IPP_ORIENT_PORTRAIT], orients[i - IPP_ORIENT_PORTRAIT]);
    }
  }
  else
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"orientation-requested-checkbox\"  >");
    for (i = IPP_ORIENT_PORTRAIT; i <= IPP_ORIENT_NONE; i ++)
    {
      papplClientHTMLPrintf(client, "<label class=\"image\"><input type=\"radio\" disabled name=\"orientation-requested\" value=\"%d\"%s> <img src=\"data:image/svg+xml,%s\" alt=\"%s\"></label> ", i, iterator_preset->orient_default == (ipp_orient_t)i ? " checked" : "", orient_svgs[i - IPP_ORIENT_PORTRAIT], orients[i - IPP_ORIENT_PORTRAIT]);
    }

  } 



  papplClientHTMLPuts(client, "</td></tr>\n");

  // print-color-mode-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-color-mode"));
  if(iterator_preset->color_default_check)
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-color-mode-checkbox\" checked >");
    if (data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME) || data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_AUTO_MONOCHROME))
    {
      papplClientHTMLPuts(client, "B&amp;W");
    }
    else
    {
      for (i = PAPPL_COLOR_MODE_AUTO; i <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; i *= 2)
      {
        if ((data.color_supported & (pappl_color_mode_t)i) && i != PAPPL_COLOR_MODE_AUTO_MONOCHROME)
        {
    keyword = _papplColorModeString((pappl_color_mode_t)i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-color-mode\"  value=\"%s\"%s> %s</label> ", keyword, (pappl_color_mode_t)i == iterator_preset->color_default ? " checked" : "", localize_keyword(client, "print-color-mode", keyword, text, sizeof(text)));
        }
      }
    }
  }
  else
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-color-mode-checkbox\" >");
      if (data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME) || data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_AUTO_MONOCHROME))
      {
        papplClientHTMLPuts(client, "B&amp;W");
      }
      else
      {
        for (i = PAPPL_COLOR_MODE_AUTO; i <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; i *= 2)
        {
          if ((data.color_supported & (pappl_color_mode_t)i) && i != PAPPL_COLOR_MODE_AUTO_MONOCHROME)
          {
      keyword = _papplColorModeString((pappl_color_mode_t)i);
      papplClientHTMLPrintf(client, "<label><input type=\"radio\" disabled name=\"print-color-mode\"  value=\"%s\"%s> %s</label> ", keyword, (pappl_color_mode_t)i == iterator_preset->color_default ? " checked" : "", localize_keyword(client, "print-color-mode", keyword, text, sizeof(text)));
          }
        }
      }

  }


  papplClientHTMLPuts(client, "</td></tr>\n");

  if (data.sides_supported && data.sides_supported != PAPPL_SIDES_ONE_SIDED)
  {
    // sides-default
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "sides"));
    if(iterator_preset->sides_default_check)
    {
      papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"sides-checkbox\" checked >");
      for (i = PAPPL_SIDES_ONE_SIDED; i <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; i *= 2)
      {
        if (data.sides_supported & (pappl_sides_t)i)
        {
    keyword = _papplSidesString((pappl_sides_t)i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"sides\"  value=\"%s\"%s> %s</label> ", keyword, (pappl_sides_t)i == iterator_preset->sides_default ? " checked" : "", localize_keyword(client, "sides", keyword, text, sizeof(text)));
        }
      }
    }
    else
    {
      papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"sides-checkbox\" >");
      for (i = PAPPL_SIDES_ONE_SIDED; i <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; i *= 2)
      {
        if (data.sides_supported & (pappl_sides_t)i)
        {
    keyword = _papplSidesString((pappl_sides_t)i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" disabled name=\"sides\"  value=\"%s\"%s> %s</label> ", keyword, (pappl_sides_t)i == iterator_preset->sides_default ? " checked" : "", localize_keyword(client, "sides", keyword, text, sizeof(text)));
        }
      }

    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // output-bin-default
  if (iterator_preset->num_bin > 0)
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "output-bin"));
    if (iterator_preset->num_bin > 1)
    {
      if(iterator_preset->bin_default_check)
      {
        papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"output-bin-checkbox\" checked >");
        papplClientHTMLPuts(client, "<select name=\"output-bin\" >");
      }
      else
      {
        papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"output-bin-checkbox\" >");
        papplClientHTMLPuts(client, "<select disabled name=\"output-bin\" >");
      }
      for (i = 0; i < iterator_preset->num_bin; i ++)
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", iterator_preset->bin[i], i == iterator_preset->bin_default ? " selected" : "", localize_keyword(client, "output-bin", iterator_preset->bin[i], text, sizeof(text)));
      papplClientHTMLPuts(client, "</select>");
    }
    else
    {
      papplClientHTMLPrintf(client, "%s", localize_keyword(client, "output-bin", iterator_preset->bin[iterator_preset->bin_default], text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // print-quality-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-quality"));
  if(iterator_preset->quality_defualt_check)
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-quality-checkbox\" checked >");
    for (i = IPP_QUALITY_DRAFT; i <= IPP_QUALITY_HIGH; i ++)
    {
      keyword = ippEnumString("print-quality", i);
      papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-quality\"  value=\"%s\"%s> %s</label> ", keyword, (ipp_quality_t)i == iterator_preset->quality_default ? " checked" : "", localize_keyword(client, "print-quality", keyword, text, sizeof(text)));
    }
  }
  else
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\" id=\"print-quality-checkbox\" >");
    for (i = IPP_QUALITY_DRAFT; i <= IPP_QUALITY_HIGH; i ++)
    {
      keyword = ippEnumString("print-quality", i);
      papplClientHTMLPrintf(client, "<label><input type=\"radio\" disabled name=\"print-quality\"  value=\"%s\"%s> %s</label> ", keyword, (ipp_quality_t)i == iterator_preset->quality_default ? " checked" : "", localize_keyword(client, "print-quality", keyword, text, sizeof(text)));
    }
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-darkness-configured
  if (data.darkness_supported)
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-darkness"));
    if(iterator_preset->darkness_configured_check)
    {
      papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-darkness-checkbox\" checked >");
      papplClientHTMLPrintf(client, "<select name=\"print-darkness\">");
    }
    else
    {
      papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-darkness-checkbox\" >");
      papplClientHTMLPrintf(client, "<select disabled name=\"print-darkness\">");
    }
    for (i = 0; i < data.darkness_supported; i ++)
    {
      int percent = 100 * i / (data.darkness_supported - 1);
					// Percent darkness

      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d%%</option>", percent, percent == iterator_preset->darkness_configured ? " selected" : "", percent);
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-speed-default
  if (data.speed_supported[1])
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th>", papplClientGetLocString(client, "print-speed"), papplClientGetLocString(client, _PAPPL_LOC("Auto")));
    if(iterator_preset->speed_defualt_check)
    {
      papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-speed-checkbox\" checked>");
      for (i = data.speed_supported[0]; i <= data.speed_supported[1]; i += 2540)
      {
        if (i > 0)
        {
          papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), i > 2540 ? _PAPPL_LOC("%d inches/sec") : _PAPPL_LOC("%d inch/sec"), i / 2540);
    papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%s</option>", i / 2540, i == iterator_preset->speed_default ? " selected" : "", text);
        }
      }
    }
    else
    {
      papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-speed-checkbox\" >");
      for (i = data.speed_supported[0]; i <= data.speed_supported[1]; i += 2540)
      {
        if (i > 0)
        {
          papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), i > 2540 ? _PAPPL_LOC("%d inches/sec") : _PAPPL_LOC("%d inch/sec"), i / 2540);
    papplClientHTMLPrintf(client, "<option disabled value=\"%d\"%s>%s</option>", i / 2540, i == iterator_preset->speed_default ? " selected" : "", text);
        }
      }
    }


    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-content-optimize-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th> <td>", papplClientGetLocString(client, "print-content-optimize"));
  if(iterator_preset->content_default_check)
  {
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-content-optimize-checkbox\" checked >");
    papplClientHTMLPrintf(client, "<select name=\"print-content-optimize\">");
  }
  else
  {
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"print-content-optimize-checkbox\" >");
    papplClientHTMLPrintf(client, "<select disabled name=\"print-content-optimize\">");
  }

  for (i = PAPPL_CONTENT_AUTO; i <= PAPPL_CONTENT_TEXT_AND_GRAPHIC; i *= 2)
  {
    keyword = _papplContentString((pappl_content_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_content_t)i == iterator_preset->content_default ? " selected" : "", localize_keyword(client, "print-content-optimize", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-scaling-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-scaling"));
  if(iterator_preset->scaling_default_check)
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\"  id=\"print-scaling-checkbox\" checked >");
    papplClientHTMLPrintf(client, "<select name=\"print-scaling\">");

  }
  else
  {
    papplClientHTMLPrintf(client, "  <input type=\"checkbox\"  id=\"print-scaling-checkbox\" >");
    papplClientHTMLPrintf(client, "<select disabled name=\"print-scaling\">");

  }
  for (i = PAPPL_SCALING_AUTO; i <= PAPPL_SCALING_NONE; i *= 2)
  {
    keyword = _papplScalingString((pappl_scaling_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_scaling_t)i == iterator_preset->scaling_default ? " selected" : "", localize_keyword(client, "print-scaling", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // printer-resolution-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "printer-resolution"));
  if(iterator_preset->x_default_check && iterator_preset->y_default_check)
  {
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"printer-resolution-checkbox\" checked>");
    if (data.num_resolution == 1)
    {
      if (data.x_resolution[0] != data.y_resolution[0])
        papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%dx%ddpi")), data.x_resolution[0], data.y_resolution[0]);
      else
        papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%ddpi")), data.x_resolution[0]);
    }
    else
    {
      papplClientHTMLPuts(client, "<select name=\"printer-resolution\">");
      for (i = 0; i < data.num_resolution; i ++)
      {
        if (data.x_resolution[i] != data.y_resolution[i])
          papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%dx%ddpi"), data.x_resolution[i], data.y_resolution[i]);
        else
    papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%ddpi"), data.x_resolution[i]);

        papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", text, (iterator_preset->x_default == data.x_resolution[i] && iterator_preset->y_default == data.y_resolution[i]) ? " selected" : "", text);
      }
      papplClientHTMLPuts(client, "</select>");
    }
  }
  else
  {
    papplClientHTMLPrintf(client, " <input type=\"checkbox\" id=\"printer-resolution-checkbox\" >");
    if (data.num_resolution == 1)
    {
      if (data.x_resolution[0] != data.y_resolution[0])
        papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%dx%ddpi")), data.x_resolution[0], data.y_resolution[0]);
      else
        papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%ddpi")), data.x_resolution[0]);
    }
    else
    {
      papplClientHTMLPuts(client, "<select disabled name=\"printer-resolution\">");
      for (i = 0; i < data.num_resolution; i ++)
      {
        if (data.x_resolution[i] != data.y_resolution[i])
          papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%dx%ddpi"), data.x_resolution[i], data.y_resolution[i]);
        else
    papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%ddpi"), data.x_resolution[i]);

        papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", text, (iterator_preset->x_default == data.x_resolution[i] && iterator_preset->y_default == data.y_resolution[i]) ? " selected" : "", text);
      }
      papplClientHTMLPuts(client, "</select>");
    }
  }



  papplClientHTMLPuts(client, "</td></tr>\n");

  // papplClientHTMLPrintf(client , "<script> function setupCheckboxSelectInteraction(checkboxId, selectName) { var checkbox = document.getElementById(checkboxId); var selectElement = document.querySelector('select[name=\"' + selectName + '\"]'); if (checkbox && selectElement) { checkbox.addEventListener('change', function() { selectElement.disabled = !checkbox.checked; }); } } function setupCheckboxRadioInteraction(checkboxId, radioName) { var checkbox = document.getElementById(checkboxId); var radioButtons = document.querySelectorAll('input[type=\"radio\"][name=\"' + radioName + '\"]'); if (checkbox && radioButtons.length > 0) { checkbox.addEventListener('change', function() { radioButtons.forEach(function(radioButton) { radioButton.disabled = !checkbox.checked; }); }); } } </script>");
  papplClientHTMLPrintf(client, "<script> function setup_Interaction(checkboxId, inputName) { var types = [\"select\", \"radio\", \"text\", \"number\", \"checkbox\"]; var checkbox = document.getElementById(checkboxId); types.forEach(function (type) { var createdPattern; switch (type) { case \"select\": createdPattern = 'select[name=\"' + inputName + '\"]'; break; case \"radio\": createdPattern = 'input[type=\"radio\"][name=\"' + inputName + '\"]'; break; case \"text\": created_pattern = 'input[type=\"text\"][name=\"' + inputName + '\"]'; case \"number\": created_pattern = 'input[type=\"number\"][name=\"' + inputName + '\"]'; case \"checkbox\": created_pattern = 'input[type=\"checkbox\"][name=\"' + inputName + '\"]'; } console.log(createdPattern); var inputs = document.querySelectorAll(createdPattern); if (checkbox && inputs.length > 0) { checkbox.addEventListener(\"change\", function () { inputs.forEach(function (input) { input.disabled = !checkbox.checked; }); }); } }); } </script>");





  // Vendor options
  _papplRWLockRead(printer);

  for (i = 0; i < data.num_vendor; i ++)
  {
    // printf("The value inside check array for this %s attribute is --> %d", data.vendor[i], iterator_preset->is_vendor[i]);

    /* initialize the variables and setting the different variables ...
    */
    char	defname[128],		// xxx-default name
		defvalue[1024],		// xxx-default value
		supname[128];		// xxx-supported name
    ipp_attribute_t *defattr,		// xxx-default attribute
	      	*supattr;		// xxx-supported attribute
    int		count;			// Number of values
    char buffer[1024];
    snprintf(defname, sizeof(defname), "%s-default", data.vendor[i]);
    snprintf(supname, sizeof(defname), "%s-supported", data.vendor[i]);


    /*
     * Here we just pullout the attribute from the preset ...
     */

    if ((defattr = ippFindAttribute(iterator_preset->driver_attrs, defname, IPP_TAG_ZERO)) != NULL)
    {
      ippAttributeString(defattr, defvalue, sizeof(defvalue));
    }
      
    else
      defvalue[0] = '\0';

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, data.vendor[i]));
    snprintf(buffer, sizeof(buffer), "%s-checkbox", data.vendor[i]);

    if(iterator_preset->is_vendor[i])
    {
      papplClientHTMLPrintf(client, "  <input type=\"checkbox\" checked id=\"%s\" >", buffer);
    }
    else
    {
      papplClientHTMLPrintf(client, "  <input type=\"checkbox\"  id=\"%s\" >", buffer);


    }


    if ((supattr = ippFindAttribute(printer->driver_attrs, supname, IPP_TAG_ZERO)) != NULL)
    {

      count = (int)ippGetCount(supattr);

      switch (ippGetValueTag(supattr))
      {
        case IPP_TAG_BOOLEAN :
            if(iterator_preset->is_vendor[i])
              papplClientHTMLPrintf(client, "<input type=\"checkbox\" name=\"%s\"%s>", data.vendor[i], !strcmp(defvalue, "true") ? " checked" : "");
            else 
              papplClientHTMLPrintf(client, "<input type=\"checkbox\" disabled name=\"%s\"%s>", data.vendor[i], !strcmp(defvalue, "true") ? " checked" : "");
            break;

        case IPP_TAG_INTEGER :
            if(iterator_preset->is_vendor[i])
            {
              papplClientHTMLPrintf(client, "<select name=\"%s\">", data.vendor[i]);

            }
            else
            {
              papplClientHTMLPrintf(client, "<select disabled name=\"%s\">", data.vendor[i]);

            }

            for (j = 0; j < count; j ++)
            {
              int val = ippGetInteger(supattr, (int)j);

	      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d</option>", val, val == (int)strtol(defvalue, NULL, 10) ? " selected" : "", val);
            }
            papplClientHTMLPuts(client, "</select>");
            break;

        case IPP_TAG_RANGE :
            {
              int upper, lower = ippGetRange(supattr, 0, &upper);
					// Range
            if(iterator_preset->is_vendor[i])
	            papplClientHTMLPrintf(client, "<input type=\"number\" name=\"%s\" min=\"%d\" max=\"%d\" value=\"%s\">", data.vendor[i], lower, upper, defvalue);
            else
	      papplClientHTMLPrintf(client, "<input type=\"number\" disabled name=\"%s\" min=\"%d\" max=\"%d\" value=\"%s\">", data.vendor[i], lower, upper, defvalue);

	    }
            break;

        case IPP_TAG_KEYWORD :
            if(iterator_preset->is_vendor[i])
              papplClientHTMLPrintf(client, "<select  name=\"%s\">", data.vendor[i]);
            else
              papplClientHTMLPrintf(client, "<select disabled name=\"%s\">", data.vendor[i]);
            for (j = 0; j < count; j ++)
            {
              const char *val = ippGetString(supattr, (int)j, NULL);

	      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", val, !strcmp(val, defvalue) ? " selected" : "", localize_keyword(client, data.vendor[i], val, text, sizeof(text)));
            }
            papplClientHTMLPuts(client, "</select>");
            break;

	default :
	    papplClientHTMLPuts(client, "Unsupported value syntax.");
	    break;
      }
    }
    else
    {
      if(iterator_preset->is_vendor[i])
        papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s\" value=\"%s\">", data.vendor[i], defvalue);

      else
        papplClientHTMLPrintf(client, "<input type=\"text\" disabled name=\"%s\" value=\"%s\">", data.vendor[i], defvalue);
    }
    // papplClientHTMLPrintf(client, "<script> setupCheckboxRadioInteraction(\"%s\", \"%s\");  setupCheckboxSelectInteraction(\"%s\", \"%s\"); </script>", buffer, data.vendor[i],buffer,data.vendor[i]);
    papplClientHTMLPrintf(client, "<script>  setup_Interaction(\"%s\", \"%s\")  </script>", buffer, data.vendor[i]);

    papplClientHTMLPuts(client, "</td></tr>\n");
  }


for (int i = 0; i < sizeof(static_attribute_names) / sizeof(static_attribute_names[0]); i++) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "%s-checkbox", static_attribute_names[i]);
        // printf("%s\n", buffer);
        papplClientHTMLPrintf(client, "<script>   console.log(\"%s\");  </script> ",buffer);
        papplClientHTMLPrintf(client, "<script> setup_Interaction(\"%s\", \"%s\"); </script>", buffer, static_attribute_names[i]);
        // papplClientHTMLPrintf(client, "<script> setupCheckboxRadioInteraction(\"%s\", \"%s\");  setupCheckboxSelectInteraction(\"%s\", \"%s\"); </script>", buffer, static_attribute_names[i],buffer, static_attribute_names[i]);
    }

  papplClientHTMLPrintf(client, "<script> document.addEventListener('DOMContentLoaded', function() { console.log('function is working fine'); var form = document.getElementById('form'); if (form) { form.addEventListener('submit', function() { var checkboxes = form.querySelectorAll('input[type=\"checkbox\"]'); checkboxes.forEach(function(checkbox) { checkbox.disabled = true; }); }); } }); </script>");


  _papplRWUnlock(printer);
// papplClientHTMLPrintf(client,
//                       "              <tr><th></th><td><input type=\"submit\" value=\"%s\" onclick=\"handleSubmit()\"></td></tr>\n" // Add onclick event to call the JavaScript function
//                       "            </tbody>\n"
//                       "          </table>"
//                       "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Save")));
  papplClientHTMLPrintf(client,
                        "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
                        "            </tbody>\n"
                        "          </table>"
                        "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Save")));



          

  papplClientHTMLPrinterFooter(client);

  
}



//
// '_papplPrinterWebDefaults()' - Show the printer defaults web page.
//

void
_papplPrinterWebDefaults(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  int			i, j;		// Looping vars
  pappl_pr_driver_data_t data;		// Driver data
  const char		*keyword;	// Current keyword
  char			text[256];	// Localized text for keyword
  const char		*status = NULL;	// Status message, if any
  bool			show_source = false;
					// Show the media source?
  static const char * const orients[] =	// orientation-requested strings
  {
    _PAPPL_LOC("Portrait"),
    _PAPPL_LOC("Landscape"),
    _PAPPL_LOC("Reverse Landscape"),
    _PAPPL_LOC("Reverse Portrait"),
    _PAPPL_LOC("Auto")
  };
  static const char * const orient_svgs[] =
  {					// orientation-requested images
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='18' font-size='18' fill='currentColor' rotate='0'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='19' font-size='18' fill='currentColor' rotate='-90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='3' y='6' font-size='18' fill='currentColor' rotate='90'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='15' y='7' font-size='18' fill='currentColor' rotate='180'%3eA%3c/text%3e%3c/svg%3e",
    "%3csvg xmlns='http://www.w3.org/2000/svg' width='18' height='24' viewBox='0 0 18 24'%3e%3crect fill='rgba(255,255,255,.5)' stroke='currentColor' stroke-width='1' x='0' y='0' width='18' height='24' rx='5' ry='5'/%3e%3ctext x='5' y='18' font-size='18' fill='currentColor' rotate='0'%3e?%3c/text%3e%3c/svg%3e"
  };


  if (!papplClientHTMLAuthorize(client))
    return;

  papplPrinterGetDriverData(printer, &data);

  if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    int			num_vendor = 0;	// Number of vendor options
    cups_option_t	*vendor = NULL;	// Vendor options

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else
    {
      const char	*value;		// Value of form variable
      char		*end;			// End of value

      if ((value = cupsGetOption("orientation-requested", num_form, form)) != NULL)
      {
        data.orient_default = (ipp_orient_t)strtol(value, &end, 10);

        if (errno == ERANGE || *end || data.orient_default < IPP_ORIENT_PORTRAIT || data.orient_default > IPP_ORIENT_NONE)
          data.orient_default = IPP_ORIENT_PORTRAIT;
      }

      if ((value = cupsGetOption("output-bin", num_form, form)) != NULL)
      {
        for (i = 0; i < data.num_bin; i ++)
        {
          if (!strcmp(data.bin[i], value))
          {
            data.bin_default = i;
            break;
          }
	}
      }

      if ((value = cupsGetOption("print-color-mode", num_form, form)) != NULL)
        data.color_default = _papplColorModeValue(value);

      if ((value = cupsGetOption("print-content-optimize", num_form, form)) != NULL)
        data.content_default = _papplContentValue(value);

      if ((value = cupsGetOption("print-darkness", num_form, form)) != NULL)
      {
        data.darkness_configured = (int)strtol(value, &end, 10);

        if (errno == ERANGE || *end || data.darkness_configured < 0 || data.darkness_configured > 100)
          data.darkness_configured = 50;
      }

      if ((value = cupsGetOption("print-quality", num_form, form)) != NULL)
        data.quality_default = (ipp_quality_t)ippEnumValue("print-quality", value);

      if ((value = cupsGetOption("print-scaling", num_form, form)) != NULL)
        data.scaling_default = _papplScalingValue(value);

      if ((value = cupsGetOption("print-speed", num_form, form)) != NULL)
      {
        data.speed_default = (int)strtol(value, &end, 10) * 2540;

        if (errno == ERANGE || *end || data.speed_default < 0 || data.speed_default > data.speed_supported[1])
          data.speed_default = 0;
      }

      if ((value = cupsGetOption("sides", num_form, form)) != NULL)
        data.sides_default = _papplSidesValue(value);

      if ((value = cupsGetOption("printer-resolution", num_form, form)) != NULL)
      {
        if (sscanf(value, "%dx%ddpi", &data.x_default, &data.y_default) == 1)
          data.y_default = data.x_default;
      }

      if ((value = cupsGetOption("media-source", num_form, form)) != NULL)
      {
        for (i = 0; i < data.num_source; i ++)
	{
	  if (!strcmp(value, data.source[i]))
	  {
	    data.media_default = data.media_ready[i];
	    break;
	  }
	}
      }

      for (i = 0; i < data.num_vendor; i ++)
      {
        char	supattr[128];		// xxx-supported

        snprintf(supattr, sizeof(supattr), "%s-supported", data.vendor[i]);

        if ((value = cupsGetOption(data.vendor[i], num_form, form)) != NULL)
	  num_vendor = (int)cupsAddOption(data.vendor[i], value, (cups_len_t)num_vendor, &vendor);
	else if (ippFindAttribute(printer->driver_attrs, supattr, IPP_TAG_BOOLEAN))
	  num_vendor = (int)cupsAddOption(data.vendor[i], "false", (cups_len_t)num_vendor, &vendor);
      }

      if (papplPrinterSetDriverDefaults(printer, &data, num_vendor, vendor))
        status = _PAPPL_LOC("Changes saved.");
      else
        status = _PAPPL_LOC("Bad printer defaults.");

      cupsFreeOptions((cups_len_t)num_vendor, vendor);
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Printing Defaults"), 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  // media-col-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "media"));

  if (data.num_source > 1)
  {
    papplClientHTMLPuts(client, "<select name=\"media-source\">");

    for (i = 0; i < data.num_source; i ++)
    {
      // See if any two sources have the same size...
      for (j = i + 1; j < data.num_source; j ++)
      {
	if (data.media_ready[i].size_width > 0 && data.media_ready[i].size_width == data.media_ready[j].size_width && data.media_ready[i].size_length == data.media_ready[j].size_length)
	{
	  show_source = true;
	  break;
	}
      }
    }

    for (i = 0; i < data.num_source; i ++)
    {
      keyword = data.source[i];

      if (strcmp(keyword, "manual"))
      {
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, !strcmp(keyword, data.media_default.source) ? " selected" : "", localize_media(client, data.media_ready + i, show_source, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</select>");
  }
  else
    papplClientHTMLEscape(client, localize_media(client, data.media_ready, false, text, sizeof(text)), 0);

  papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s/media\">%s</a></td></tr>\n", printer->uriname, papplClientGetLocString(client, _PAPPL_LOC("Configure Media")));

  // orientation-requested-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "orientation-requested"));
  for (i = IPP_ORIENT_PORTRAIT; i <= IPP_ORIENT_NONE; i ++)
  {
    papplClientHTMLPrintf(client, "<label class=\"image\"><input type=\"radio\" name=\"orientation-requested\" value=\"%d\"%s> <img src=\"data:image/svg+xml,%s\" alt=\"%s\"></label> ", i, data.orient_default == (ipp_orient_t)i ? " checked" : "", orient_svgs[i - IPP_ORIENT_PORTRAIT], orients[i - IPP_ORIENT_PORTRAIT]);
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // print-color-mode-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-color-mode"));
  if (data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME) || data.color_supported == (PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_AUTO_MONOCHROME))
  {
    papplClientHTMLPuts(client, "B&amp;W");
  }
  else
  {
    for (i = PAPPL_COLOR_MODE_AUTO; i <= PAPPL_COLOR_MODE_PROCESS_MONOCHROME; i *= 2)
    {
      if ((data.color_supported & (pappl_color_mode_t)i) && i != PAPPL_COLOR_MODE_AUTO_MONOCHROME)
      {
	keyword = _papplColorModeString((pappl_color_mode_t)i);
	papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-color-mode\" value=\"%s\"%s> %s</label> ", keyword, (pappl_color_mode_t)i == data.color_default ? " checked" : "", localize_keyword(client, "print-color-mode", keyword, text, sizeof(text)));
      }
    }
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  if (data.sides_supported && data.sides_supported != PAPPL_SIDES_ONE_SIDED)
  {
    // sides-default
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "sides"));
    for (i = PAPPL_SIDES_ONE_SIDED; i <= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE; i *= 2)
    {
      if (data.sides_supported & (pappl_sides_t)i)
      {
	keyword = _papplSidesString((pappl_sides_t)i);
	papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"sides\" value=\"%s\"%s> %s</label> ", keyword, (pappl_sides_t)i == data.sides_default ? " checked" : "", localize_keyword(client, "sides", keyword, text, sizeof(text)));
      }
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // output-bin-default
  if (data.num_bin > 0)
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "output-bin"));
    if (data.num_bin > 1)
    {
      papplClientHTMLPuts(client, "<select name=\"output-bin\">");
      for (i = 0; i < data.num_bin; i ++)
	papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", data.bin[i], i == data.bin_default ? " selected" : "", localize_keyword(client, "output-bin", data.bin[i], text, sizeof(text)));
      papplClientHTMLPuts(client, "</select>");
    }
    else
    {
      papplClientHTMLPrintf(client, "%s", localize_keyword(client, "output-bin", data.bin[data.bin_default], text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  // print-quality-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "print-quality"));
  for (i = IPP_QUALITY_DRAFT; i <= IPP_QUALITY_HIGH; i ++)
  {
    keyword = ippEnumString("print-quality", i);
    papplClientHTMLPrintf(client, "<label><input type=\"radio\" name=\"print-quality\" value=\"%s\"%s> %s</label> ", keyword, (ipp_quality_t)i == data.quality_default ? " checked" : "", localize_keyword(client, "print-quality", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-darkness-configured
  if (data.darkness_supported)
  {
    int darkness_value = ((data.darkness_supported - 1) * data.darkness_configured + 50) / 100;
					// Scaled darkness value

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td><select name=\"print-darkness\">", papplClientGetLocString(client, "print-darkness"));
    for (i = 0; i < data.darkness_supported; i ++)
    {
      int percent = 100 * i / (data.darkness_supported - 1);
					// Percent darkness

      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d%%</option>", percent, i == darkness_value ? " selected" : "", percent);
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-speed-default
  if (data.speed_supported[1])
  {
    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td><select name=\"print-speed\"><option value=\"0\">%s</option>", papplClientGetLocString(client, "print-speed"), papplClientGetLocString(client, _PAPPL_LOC("Auto")));
    for (i = data.speed_supported[0]; i <= data.speed_supported[1]; i += 2540)
    {
      if (i > 0)
      {
        papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), i > 2540 ? _PAPPL_LOC("%d inches/sec") : _PAPPL_LOC("%d inch/sec"), i / 2540);
	papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%s</option>", i / 2540, i == data.speed_default ? " selected" : "", text);
      }
    }
    papplClientHTMLPuts(client, "</select></td></tr>\n");
  }

  // print-content-optimize-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td><select name=\"print-content-optimize\">", papplClientGetLocString(client, "print-content-optimize"));
  for (i = PAPPL_CONTENT_AUTO; i <= PAPPL_CONTENT_TEXT_AND_GRAPHIC; i *= 2)
  {
    keyword = _papplContentString((pappl_content_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_content_t)i == data.content_default ? " selected" : "", localize_keyword(client, "print-content-optimize", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // print-scaling-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td><select name=\"print-scaling\">", papplClientGetLocString(client, "print-scaling"));
  for (i = PAPPL_SCALING_AUTO; i <= PAPPL_SCALING_NONE; i *= 2)
  {
    keyword = _papplScalingString((pappl_scaling_t)i);
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", keyword, (pappl_scaling_t)i == data.scaling_default ? " selected" : "", localize_keyword(client, "print-scaling", keyword, text, sizeof(text)));
  }
  papplClientHTMLPuts(client, "</select></td></tr>\n");

  // printer-resolution-default
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, "printer-resolution"));

  if (data.num_resolution == 1)
  {
    if (data.x_resolution[0] != data.y_resolution[0])
      papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%dx%ddpi")), data.x_resolution[0], data.y_resolution[0]);
    else
      papplClientHTMLPrintf(client, papplClientGetLocString(client, _PAPPL_LOC("%ddpi")), data.x_resolution[0]);
  }
  else
  {
    papplClientHTMLPuts(client, "<select name=\"printer-resolution\">");
    for (i = 0; i < data.num_resolution; i ++)
    {
      if (data.x_resolution[i] != data.y_resolution[i])
        papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%dx%ddpi"), data.x_resolution[i], data.y_resolution[i]);
      else
	papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC("%ddpi"), data.x_resolution[i]);

      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", text, (data.x_default == data.x_resolution[i] && data.y_default == data.y_resolution[i]) ? " selected" : "", text);
    }
    papplClientHTMLPuts(client, "</select>");
  }
  papplClientHTMLPuts(client, "</td></tr>\n");

  // Vendor options
  _papplRWLockRead(printer);

  for (i = 0; i < data.num_vendor; i ++)
  {
    char	defname[128],		// xxx-default name
		defvalue[1024],		// xxx-default value
		supname[128];		// xxx-supported name
    ipp_attribute_t *defattr,		// xxx-default attribute
	      	*supattr;		// xxx-supported attribute
    int		count;			// Number of values

    snprintf(defname, sizeof(defname), "%s-default", data.vendor[i]);
    snprintf(supname, sizeof(defname), "%s-supported", data.vendor[i]);

    if ((defattr = ippFindAttribute(printer->driver_attrs, defname, IPP_TAG_ZERO)) != NULL)
      ippAttributeString(defattr, defvalue, sizeof(defvalue));
    else
      defvalue[0] = '\0';

    papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", papplClientGetLocString(client, data.vendor[i]));

    if ((supattr = ippFindAttribute(printer->driver_attrs, supname, IPP_TAG_ZERO)) != NULL)
    {
      count = (int)ippGetCount(supattr);

      switch (ippGetValueTag(supattr))
      {
        case IPP_TAG_BOOLEAN :
            papplClientHTMLPrintf(client, "<input type=\"checkbox\" name=\"%s\"%s>", data.vendor[i], !strcmp(defvalue, "true") ? " checked" : "");
            break;

        case IPP_TAG_INTEGER :
            papplClientHTMLPrintf(client, "<select name=\"%s\">", data.vendor[i]);
            for (j = 0; j < count; j ++)
            {
              int val = ippGetInteger(supattr, (cups_len_t)j);

	      papplClientHTMLPrintf(client, "<option value=\"%d\"%s>%d</option>", val, val == (int)strtol(defvalue, NULL, 10) ? " selected" : "", val);
            }
            papplClientHTMLPuts(client, "</select>");
            break;

        case IPP_TAG_RANGE :
            {
              int upper, lower = ippGetRange(supattr, 0, &upper);
					// Range

	      papplClientHTMLPrintf(client, "<input type=\"number\" name=\"%s\" min=\"%d\" max=\"%d\" value=\"%s\">", data.vendor[i], lower, upper, defvalue);
	    }
            break;

        case IPP_TAG_KEYWORD :
            papplClientHTMLPrintf(client, "<select name=\"%s\">", data.vendor[i]);
            for (j = 0; j < count; j ++)
            {
              const char *val = ippGetString(supattr, (cups_len_t)j, NULL);

	      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", val, !strcmp(val, defvalue) ? " selected" : "", localize_keyword(client, data.vendor[i], val, text, sizeof(text)));
            }
            papplClientHTMLPuts(client, "</select>");
            break;

	default :
	    papplClientHTMLPuts(client, "Unsupported value syntax.");
	    break;
      }
    }
    else
    {
      // Text option
      papplClientHTMLPrintf(client, "<input type=\"text\" name=\"%s\" value=\"%s\">", data.vendor[i], defvalue);
    }

    papplClientHTMLPuts(client, "</td></tr>\n");
  }

  _papplRWUnlock(printer);

  papplClientHTMLPrintf(client,
                        "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
                        "            </tbody>\n"
                        "          </table>"
                        "        </form>\n", papplClientGetLocString(client, _PAPPL_LOC("Save Changes")));

  papplClientHTMLPrinterFooter(client);
}


//
// '_papplPrinterWebDelete()' - Show the printer delete confirmation web page.
//

void
_papplPrinterWebDelete(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  const char	*status = NULL;		// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t		num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if (printer->processing_job)
    {
      // Printer is processing a job...
      status = _PAPPL_LOC("Printer is currently active.");
    }
    else
    {
      if (!papplPrinterIsDeleted(printer))
      {
        papplPrinterDelete(printer);
        printer = NULL;
      }

      papplClientRespondRedirect(client, HTTP_STATUS_FOUND, "/");
      cupsFreeOptions(num_form, form);
      return;
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Delete Printer"), 0, NULL, NULL);

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);
  papplClientHTMLPrintf(client,"          <input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Confirm Delete Printer")));

  papplClientHTMLFooter(client);
}


//
// '_papplPrinterWebHome()' - Show the printer home page.
//

void
_papplPrinterWebHome(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  const char	*status = NULL;		// Status text, if any
  ipp_pstate_t	printer_state;		// Printer state
  char		edit_path[1024];	// Edit configuration URL
  const int	limit = 20;		// Jobs per page
  int		job_index = 1;		// Job index
  char		dns_sd_name[64],	// Printer DNS-SD name
		location[128],		// Printer location
		geo_location[128],	// Printer geo-location
		organization[256],	// Printer organization
		org_unit[256];		// Printer organizational unit
  pappl_contact_t contact;		// Printer contact


  // Save current printer state...
  printer_state = papplPrinterGetState(printer);

  // Handle POSTs to print a test page...
  if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    const char		*action;	// Form action

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if ((action = cupsGetOption("action", num_form, form)) == NULL)
    {
      status = _PAPPL_LOC("Missing action.");
    }
    else if (!strcmp(action, "hold-new-jobs"))
    {
      papplPrinterHoldNewJobs(printer);

      status = _PAPPL_LOC("Holding new jobs.");
    }
    else if (!strcmp(action, "identify-printer"))
    {
      if (printer->driver_data.identify_supported && printer->driver_data.identify_cb)
      {
        (printer->driver_data.identify_cb)(printer, printer->driver_data.identify_supported, "Hello.");

        status = _PAPPL_LOC("Printer identified.");
      }
      else
      {
        status = _PAPPL_LOC("Unable to identify printer.");
      }
    }
    else if (!strcmp(action, "print-test-page"))
    {
      pappl_job_t	*job;		// New job
      const char	*filename,	// Test Page filename
			*username;	// Username
      char		buffer[1024];	// File Buffer

      // Get the testfile to print, if any...
      if (printer->driver_data.testpage_cb)
        filename = (printer->driver_data.testpage_cb)(printer, buffer, sizeof(buffer));
      else
	filename = NULL;

      if (filename)
      {
        // Have a file to print, so create a job and print it...
        if (client->username[0])
          username = client->username;
        else
          username = "guest";

        if (access(filename, R_OK))
        {
          status = _PAPPL_LOC("Unable to access test print file.");
        }
        else if ((job = _papplJobCreate(printer, 0, username, NULL, "Test Page", NULL)) == NULL)
        {
          status = _PAPPL_LOC("Unable to create test print job.");
        }
        else
        {
          // Submit the job for processing...
          _papplJobSubmitFile(job, filename);

          status        = _PAPPL_LOC("Test page printed.");
          printer_state = IPP_PSTATE_PROCESSING;
        }
      }
      else
      {
        status        = _PAPPL_LOC("Test page printed.");
        printer_state = IPP_PSTATE_PROCESSING;
      }
    }
    else if (!strcmp(action, "pause-printer"))
    {
      papplPrinterPause(printer);

      if (printer->state == IPP_PSTATE_STOPPED)
        status = _PAPPL_LOC("Printer paused.");
      else
        status = _PAPPL_LOC("Printer pausing.");
    }
    else if (!strcmp(action, "release-held-new-jobs"))
    {
      papplPrinterReleaseHeldNewJobs(printer, client->username);

      status = _PAPPL_LOC("Released held new jobs.");
    }
    else if (!strcmp(action, "resume-printer"))
    {
      papplPrinterResume(printer);

      status = _PAPPL_LOC("Printer resuming.");
    }
    else if (!strcmp(action, "set-as-default"))
    {
      papplSystemSetDefaultPrinterID(printer->system, printer->printer_id);
      status = _PAPPL_LOC("Default printer set.");
    }
    else
      status = _PAPPL_LOC("Unknown action.");

    cupsFreeOptions(num_form, form);
  }

  // Show status...
  papplClientHTMLPrinterHeader(client, printer, NULL, printer_state == IPP_PSTATE_PROCESSING ? 10 : 0, NULL, NULL);

  papplClientHTMLPuts(client,
                      "      <div class=\"row\">\n"
                      "        <div class=\"col-6\">\n");

  _papplPrinterWebIteratorCallback(printer, client);

  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  snprintf(edit_path, sizeof(edit_path), "%s/config", printer->uriname);
  papplClientHTMLPrintf(client, "          <h1 class=\"title\">%s <a class=\"btn\" href=\"%s://%s:%d%s\">%s</a></h1>\n", papplClientGetLocString(client, _PAPPL_LOC("Configuration")), _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, edit_path, papplClientGetLocString(client, _PAPPL_LOC("Change")));

  _papplClientHTMLPutLinks(client, printer->links, PAPPL_LOPTIONS_CONFIGURATION);

  _papplClientHTMLInfo(client, false, papplPrinterGetDNSSDName(printer, dns_sd_name, sizeof(dns_sd_name)), papplPrinterGetLocation(printer, location, sizeof(location)), papplPrinterGetGeoLocation(printer, geo_location, sizeof(geo_location)), papplPrinterGetOrganization(printer, organization, sizeof(organization)), papplPrinterGetOrganizationalUnit(printer, org_unit, sizeof(org_unit)), papplPrinterGetContact(printer, &contact));

  if (!(printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    _papplSystemWebSettings(client);

  papplClientHTMLPrintf(client,
			"        </div>\n"
			"        <div class=\"col-6\">\n"
			"          <h1 class=\"title\"><a href=\"%s/jobs\">%s</a>", printer->uriname, papplClientGetLocString(client, _PAPPL_LOC("Jobs")));

  if (papplPrinterGetNumberOfJobs(printer) > 0)
  {
    if (cupsArrayGetCount(printer->active_jobs) > 0)
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s://%s:%d%s/cancelall\">%s</a></h1>\n", _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, printer->uriname, papplClientGetLocString(client, _PAPPL_LOC("Cancel All Jobs")));
    else
      papplClientHTMLPuts(client, "</h1>\n");

    _papplClientHTMLPutLinks(client, printer->links, PAPPL_LOPTIONS_JOB);

    job_pager(client, printer, job_index, limit);

    papplClientHTMLPrintf(client,
			  "          <table class=\"list\" summary=\"Jobs\">\n"
			  "            <thead>\n"
			  "              <tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th></th></tr>\n"
			  "            </thead>\n"
			  "            <tbody>\n", papplClientGetLocString(client, _PAPPL_LOC("Job #")), papplClientGetLocString(client, _PAPPL_LOC("Name")), papplClientGetLocString(client, _PAPPL_LOC("Owner")), papplClientGetLocString(client, _PAPPL_LOC("Pages")), papplClientGetLocString(client, _PAPPL_LOC("Status")));

    papplPrinterIterateAllJobs(printer, (pappl_job_cb_t)job_cb, client, job_index, limit);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");

    job_pager(client, printer, job_index, limit);
  }
  else
  {
    papplClientHTMLPuts(client, "</h1>\n");
    _papplClientHTMLPutLinks(client, printer->links, PAPPL_LOPTIONS_JOB);
    papplClientHTMLPrintf(client, "        <p>%s</p>\n", papplClientGetLocString(client, _PAPPL_LOC("No jobs in history.")));
  }

  papplClientHTMLPrinterFooter(client);
}


//
// '_papplPrinterWebIteratorCallback()' - Show the printer status.
//

void
_papplPrinterWebIteratorCallback(
    pappl_printer_t *printer,		// I - Printer
    pappl_client_t  *client)		// I - Client
{
  pappl_preason_t	reason,		// Current reason
			printer_reasons;// Printer state reasons
  ipp_pstate_t		printer_state;	// Printer state
  int			printer_jobs;	// Number of queued jobs
  char			state_str[8],	// State string
			jobs_str[256],	// Number of jobs string
			uri[256],	// Form URI
			text[1024];	// Localized text


  printer_jobs    = papplPrinterGetNumberOfActiveJobs(printer);
  printer_state   = papplPrinterGetState(printer);
  printer_reasons = papplPrinterGetReasons(printer);

  snprintf(uri, sizeof(uri), "%s/", printer->uriname);

  if (!strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client,
			  "          <h2 class=\"title\"><a href=\"%s/\">%s</a> <a class=\"btn\" href=\"%s://%s:%d%s/delete\">%s</a></h2>\n", printer->uriname, printer->name, _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, printer->uriname, papplClientGetLocString(client, _PAPPL_LOC("Delete")));
  else
    papplClientHTMLPrintf(client, "          <h1 class=\"title\">%s</h1>\n", papplClientGetLocString(client, _PAPPL_LOC("Status")));

  snprintf(state_str, sizeof(state_str), "%d", (int)printer_state);
  papplLocFormatString(papplClientGetLoc(client), jobs_str, sizeof(jobs_str), printer_jobs == 1 ? _PAPPL_LOC("%d job") : _PAPPL_LOC("%d jobs"), printer_jobs);

  papplClientHTMLPrintf(client,
			"          <p><img class=\"%s\" src=\"%s/icon-md.png\">%s, %s", ippEnumString("printer-state", (int)printer_state), printer->uriname, localize_keyword(client, "printer-state", state_str, text, sizeof(text)), jobs_str);
  if ((printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE) && printer->printer_id == printer->system->default_printer_id)
    papplClientHTMLPrintf(client, ", %s", papplClientGetLocString(client, _PAPPL_LOC("default printer")));
  if (printer->hold_new_jobs)
    papplClientHTMLPrintf(client, ", %s", papplClientGetLocString(client, _PAPPL_LOC("holding new jobs")));
  for (reason = PAPPL_PREASON_OTHER; reason <= PAPPL_PREASON_TONER_LOW; reason *= 2)
  {
    if (printer_reasons & reason)
      papplClientHTMLPrintf(client, ", %s", localize_keyword(client, "printer-state-reasons", _papplPrinterReasonString(reason), text, sizeof(text)));
  }

  if (strcmp(printer->name, printer->driver_data.make_and_model))
    papplClientHTMLPrintf(client, ".<br>%s</p>\n", printer->driver_data.make_and_model);
  else
    papplClientHTMLPuts(client, ".</p>\n");

  papplClientHTMLPuts(client, "          <div class=\"btn\">");
  _papplClientHTMLPutLinks(client, printer->links, PAPPL_LOPTIONS_STATUS);

  if (!printer->hold_new_jobs && papplPrinterGetMaxActiveJobs(printer) != 1)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"hold-new-jobs\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Hold New Jobs")));
  }

  if (printer->driver_data.identify_supported)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"identify-printer\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Identify Printer")));
  }

  if (printer->driver_data.testpage_cb)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"print-test-page\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Print Test Page")));
  }

  if (printer->hold_new_jobs && papplPrinterGetMaxActiveJobs(printer) != 1)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"release-held-new-jobs\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Release Held New Jobs")));
  }

  if (printer->system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    if (printer->state == IPP_PSTATE_STOPPED)
    {
      papplClientHTMLStartForm(client, uri, false);
      papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"resume-printer\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Resume Printing")));
    }
    else
    {
      papplClientHTMLStartForm(client, uri, false);
      papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"pause-printer\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Pause Printing")));
    }

    if (printer->printer_id != printer->system->default_printer_id)
    {
      papplClientHTMLStartForm(client, uri, false);
      papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"set-as-default\"><input type=\"submit\" value=\"%s\"></form>", papplClientGetLocString(client, _PAPPL_LOC("Set as Default")));
    }
  }

  if (strcmp(client->uri, "/") && (client->system->options & PAPPL_SOPTIONS_MULTI_QUEUE))
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s://%s:%d%s/delete\">%s</a>", _papplClientGetAuthWebScheme(client), client->host_field, client->host_port, printer->uriname, papplClientGetLocString(client, _PAPPL_LOC("Delete Printer")));

  papplClientHTMLPuts(client, "<br clear=\"all\"></div>\n");
}


//
// '_papplPrinterWebJobs()' - Show the printer jobs web page.
//

void
_papplPrinterWebJobs(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  ipp_pstate_t	printer_state;		// Printer state
  int		job_index = 1,		// Job index
		limit = 20;		// Jobs per page
  const char	*status = NULL;		// Status message
  bool		refresh;		// Refresh the window?

  if (!papplClientHTMLAuthorize(client))
    return;

  printer_state = papplPrinterGetState(printer);
  refresh       = printer_state == IPP_PSTATE_PROCESSING;

  if (client->operation == HTTP_STATE_GET)
  {
    cups_option_t	*form = NULL;	// Form variables
    cups_len_t		num_form = (cups_len_t)papplClientGetForm(client, &form);
					// Number of form variables
    const char		*value = NULL;	// Value of form variable

    if ((value = cupsGetOption("job-index", num_form, form)) != NULL)
      job_index = (int)strtol(value, NULL, 10);

    cupsFreeOptions(num_form, form);
  }
  else if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables
    const char		*value;		// Value of form variable
    int			job_id = 0;	// Job ID to cancel
    pappl_job_t		*job;		// Job to cancel
    const char		*action;	// Form action

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else if ((value = cupsGetOption("job-id", num_form, form)) != NULL)
    {
      char *end;			// End of value

      job_id = (int)strtol(value, &end, 10);

      if (errno == ERANGE || *end)
      {
        status = _PAPPL_LOC("Invalid job ID.");
      }
      else if ((job = papplPrinterFindJob(printer, job_id)) != NULL)
      {
        const char	*username;	// Username

        if (client->username[0])
          username = client->username;
        else
          username = "guest";

	if ((action = cupsGetOption("action", num_form, form)) == NULL)
	{
	  status = _PAPPL_LOC("Missing action.");
	}
	else if (!strcmp(action, "cancel-job"))
	{
	  papplJobCancel(job);
	  status = _PAPPL_LOC("Job canceled.");
	}
	else if (!strcmp(action, "hold-job"))
	{
	  papplJobHold(job, username, "indefinite", 0);
	  status = _PAPPL_LOC("Job held.");
	}
	else if (!strcmp(action, "release-job"))
	{
	  papplJobRelease(job, username);
	  status  = _PAPPL_LOC("Job released.");
	  refresh = true;
	}
	else if (!strcmp(action, "reprint-job"))
	{
	  // Copy the job...
	  pappl_job_t	*new_job;	// New job

	  if ((new_job = _papplJobCreate(printer, 0, username, job->format, job->name, job->attrs)) != NULL)
	  {
	    // Copy the job file...
	    int		oldfd,		// Old job file
			newfd;		// New job file
	    char	filename[1024],	// Job filename
			buffer[8192];	// Copy buffer
	    ssize_t	bytes;		// Bytes read...

	    if ((oldfd = open(job->filename, O_RDONLY | O_BINARY)) >= 0)
	    {
	      if ((newfd = papplJobOpenFile(new_job, filename, sizeof(filename), printer->system->directory, NULL, "w")) >= 0)
	      {
		while ((bytes = read(oldfd, buffer, sizeof(buffer))) > 0)
		  write(newfd, buffer, (size_t)bytes);

		close(oldfd);
		close(newfd);

		// Submit the job for processing...
		_papplJobSubmitFile(new_job, filename);
		status  = _PAPPL_LOC("Reprinted job.");
		refresh = true;
	      }

	      close(oldfd);
	    }
	  }

          if (!status)
	    status = _PAPPL_LOC("Unable to copy print job.");
	}
	else
	{
	  papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "action='%s'", action);
	  status = _PAPPL_LOC("Unknown action.");
	}
      }
      else
      {
        status = _PAPPL_LOC("Invalid Job ID.");
      }
    }
    else
    {
      status = _PAPPL_LOC("Missing job ID.");
    }

    cupsFreeOptions(num_form, form);
  }

  if (cupsArrayGetCount(printer->active_jobs) > 0)
  {
    char	url[1024];		// URL for Cancel All Jobs

    httpAssembleURIf(HTTP_URI_CODING_ALL, url, sizeof(url), "https", NULL, client->host_field, client->host_port, "%s/cancelall", printer->uriname);

    papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Jobs"), refresh ? 10 : 0, _PAPPL_LOC("Cancel All Jobs"), url);
  }
  else
  {
    papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Jobs"), printer_state == IPP_PSTATE_PROCESSING ? 10 : 0, NULL, NULL);
  }

  if (status)
    papplClientHTMLPrintf(client,
                          "      <div class=\"row\">\n"
			  "        <div class=\"col-6\">\n"
			  "          <div class=\"banner\">%s</div>\n"
			  "        </div>\n"
			  "      </div>\n", papplClientGetLocString(client, status));

  if (papplPrinterGetNumberOfJobs(printer) > 0)
  {
    job_pager(client, printer, job_index, limit);

    papplClientHTMLPrintf(client,
			  "          <table class=\"list\" summary=\"Jobs\">\n"
			  "            <thead>\n"
			  "              <tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th></th></tr>\n"
			  "            </thead>\n"
			  "            <tbody>\n", papplClientGetLocString(client, _PAPPL_LOC("Job #")), papplClientGetLocString(client, _PAPPL_LOC("Name")), papplClientGetLocString(client, _PAPPL_LOC("Owner")), papplClientGetLocString(client, _PAPPL_LOC("Pages Completed")), papplClientGetLocString(client, _PAPPL_LOC("Status")));

    papplPrinterIterateAllJobs(printer, (pappl_job_cb_t)job_cb, client, job_index, limit);

    papplClientHTMLPuts(client,
			"            </tbody>\n"
			"          </table>\n");

    job_pager(client, printer, job_index, limit);
  }
  else
    papplClientHTMLPrintf(client, "        <p>%s</p>\n", papplClientGetLocString(client, _PAPPL_LOC("No jobs in history.")));

  papplClientHTMLPrinterFooter(client);
}


//
// '_papplPrinterWebMedia()' - Show the printer media web page.
//

void
_papplPrinterWebMedia(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  int			i;		// Looping var
  pappl_pr_driver_data_t data;		// Driver data
  char			name[32],	// Prefix (readyN)
			text[256];	// Localized media-source name
  const char		*status = NULL;	// Status message, if any


  if (!papplClientHTMLAuthorize(client))
    return;

  papplPrinterGetDriverData(printer, &data);

  if (client->operation == HTTP_STATE_POST)
  {
    cups_len_t		num_form = 0;	// Number of form variable
    cups_option_t	*form = NULL;	// Form variables

    if ((num_form = (cups_len_t)papplClientGetForm(client, &form)) == 0)
    {
      status = _PAPPL_LOC("Invalid form data.");
    }
    else if (!papplClientIsValidForm(client, (int)num_form, form))
    {
      status = _PAPPL_LOC("Invalid form submission.");
    }
    else
    {
      pwg_media_t	*pwg = NULL;	// PWG media info
      pappl_media_col_t	*ready;		// Current ready media
      const char	*value,		// Value of form variable
			*custom_width,	// Custom media width
			*custom_length,	// Custom media length
			*custom_units;	// Custom media units

      memset(data.media_ready, 0, sizeof(data.media_ready));
      for (i = 0, ready = data.media_ready; i < data.num_source; i ++, ready ++)
      {
        // size
        snprintf(name, sizeof(name), "ready%d-size", i);
        if ((value = cupsGetOption(name, num_form, form)) == NULL)
          continue;

        if (!strcmp(value, "custom"))
        {
          // Custom size...
          snprintf(name, sizeof(name), "ready%d-custom-width", i);
          custom_width = cupsGetOption(name, num_form, form);
          snprintf(name, sizeof(name), "ready%d-custom-length", i);
          custom_length = cupsGetOption(name, num_form, form);
          snprintf(name, sizeof(name), "ready%d-custom-units", i);
          custom_units = cupsGetOption(name, num_form, form);

          if (custom_width && custom_length && custom_units)
          {
            if (!strcmp(custom_units, "in"))
	      pwg = pwgMediaForSize((int)(2540.0 * strtod(custom_width, NULL)), (int)(2540.0 * strtod(custom_length, NULL)));
	    else
	      pwg = pwgMediaForSize((int)(100.0 * strtod(custom_width, NULL)), (int)(100.0 * strtod(custom_length, NULL)));
	  }
        }
        else
        {
          // Standard size...
          pwg = pwgMediaForPWG(value);
        }

        papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s='%s',%d,%d", name, pwg ? pwg->pwg : "unknown", pwg ? pwg->width : 0, pwg ? pwg->length : 0);

        if (pwg)
        {
          papplCopyString(ready->size_name, pwg->pwg, sizeof(ready->size_name));
          ready->size_width  = pwg->width;
          ready->size_length = pwg->length;
        }

        // source
        papplCopyString(ready->source, data.source[i], sizeof(ready->source));

        // margins
        snprintf(name, sizeof(name), "ready%d-borderless", i);
        if (cupsGetOption(name, num_form, form))
	{
	  ready->bottom_margin = ready->top_margin = 0;
	  ready->left_margin = ready->right_margin = 0;
	}
	else
	{
	  ready->bottom_margin = ready->top_margin = data.bottom_top;
	  ready->left_margin = ready->right_margin = data.left_right;
	}

        // left-offset
        snprintf(name, sizeof(name), "ready%d-left-offset", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          ready->left_offset = (int)(100.0 * strtod(value, NULL));

        // top-offset
        snprintf(name, sizeof(name), "ready%d-top-offset", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          ready->top_offset = (int)(100.0 * strtod(value, NULL));

        // tracking
        snprintf(name, sizeof(name), "ready%d-tracking", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          ready->tracking = _papplMediaTrackingValue(value);

        // type
        snprintf(name, sizeof(name), "ready%d-type", i);
        if ((value = cupsGetOption(name, num_form, form)) != NULL)
          papplCopyString(ready->type, value, sizeof(ready->type));
      }

      papplPrinterSetReadyMedia(printer, data.num_source, data.media_ready);

      status = _PAPPL_LOC("Changes saved.");
    }

    cupsFreeOptions(num_form, form);
  }

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Media"), 0, NULL, NULL);
  if (status)
    papplClientHTMLPrintf(client, "<div class=\"banner\">%s</div>\n", papplClientGetLocString(client, status));

  papplClientHTMLStartForm(client, client->uri, false);

  papplClientHTMLPuts(client,
		      "          <table class=\"form\">\n"
		      "            <tbody>\n");

  for (i = 0; i < data.num_source; i ++)
  {
    if (!strcmp(data.source[i], "manual"))
      continue;

    snprintf(name, sizeof(name), "ready%d", i);
    media_chooser(client, &data, localize_keyword(client, "media-source", data.source[i], text, sizeof(text)), name, data.media_ready + i);
  }

  papplClientHTMLPrintf(client,
                        "              <tr><th></th><td><input type=\"submit\" value=\"%s\"></td></tr>\n"
                        "            </tbody>\n"
                        "          </table>"
		        "        </form>\n"
		        "        <script>function show_hide_custom(name) {\n"
		        "  let selelem = document.forms['form'][name + '-size'];\n"
		        "  let divelem = document.getElementById(name + '-custom');\n"
		        "  if (selelem.selectedIndex == 0)\n"
		        "    divelem.style = 'display: inline-block;';\n"
		        "  else\n"
		        "    divelem.style = 'display: none;';\n"
		        "}</script>\n", papplClientGetLocString(client, _PAPPL_LOC("Save Changes")));

  papplClientHTMLPrinterFooter(client);
}


//
// '_papplPrinterWebSupplies()' - Show the printer supplies web page.
//

void
_papplPrinterWebSupplies(
    pappl_client_t  *client,		// I - Client
    pappl_printer_t *printer)		// I - Printer
{
  int		i,			// Looping var
		num_supply;		// Number of supplies
  pappl_supply_t supply[100];		// Supplies
  static const char * const backgrounds[] =
  {
    "url(data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAwAAAAMCAYAAABWdVznAAAAAXNSR0IArs4c"
      "6QAAAERlWElmTU0AKgAAAAgAAYdpAAQAAAABAAAAGgAAAAAAA6ABAAMAAAAB"
      "AAEAAKACAAQAAAABAAAADKADAAQAAAABAAAADAAAAAATDPpdAAAAaUlEQVQo"
      "FY2R0Q3AIAhEa7siCet0HeKQtGeiwWKR+wH0HWAsRKTHK2ZGWEpExvmJLAuD"
      "LbXWNgHFV7Zzv2sTemHjCsYmS8MfjIbOEMHOsIMnQwYehiwMw6WqNxKr6F/c"
      "oyMYm0yGHYwtHq4fKZD9DnawAAAAAElFTkSuQmCC)",
					// no-color
    "#222",				// black - not 100% black for dark mode UI
    "#0FF",				// cyan
    "#777",				// gray
    "#0C0",				// green
    "#7FF",				// light-cyan
    "#CCC",				// light-gray
    "#FCF",				// light-magenta
    "#F0F",				// magenta
    "#F70",				// orange
    "#707",				// violet
    "#FF0"				// yellow
  };


  num_supply = papplPrinterGetSupplies(printer, (int)(sizeof(supply) / sizeof(supply[0])), supply);

  papplClientHTMLPrinterHeader(client, printer, _PAPPL_LOC("Supplies"), 0, NULL, NULL);

  papplClientHTMLPuts(client,
		      "          <table class=\"meter\" summary=\"Supplies\">\n"
		      "            <thead>\n"
		      "              <tr><th></th><td></td><td></td><td></td><td></td></tr>\n"
		      "            </thead>\n"
		      "            <tbody>\n");

  for (i = 0; i < num_supply; i ++)
  {
    papplClientHTMLPrintf(client, "<tr><th>%s</th><td colspan=\"4\"><span class=\"bar\" style=\"background: %s; padding: 0px %.1f%%;\" title=\"%d%%\"></span><span class=\"bar\" style=\"background: transparent; padding: 0px %.1f%%;\" title=\"%d%%\"></span></td></tr>\n", supply[i].description, backgrounds[supply[i].color], supply[i].level * 0.5, supply[i].level, 50.0 - supply[i].level * 0.5, supply[i].level);
  }

  papplClientHTMLPuts(client,
                      "            </tbody>\n"
                      "            <tfoot>\n"
                      "              <tr><th></th><td></td><td></td><td></td><td></td></tr>\n"
                      "            </tfoot>\n"
                      "          </table>\n");

  papplClientHTMLPrinterFooter(client);
}


//
// 'job_cb()' - Job iterator callback.
//

static void
job_cb(pappl_job_t    *job,		// I - Job
       pappl_client_t *client)		// I - Client
{
  bool	show_cancel = false,		// Show the "cancel" button?
	show_hold = false,		// Show the "hold" button?
	show_release = false;		// Show the "release" button?
  char	uri[256],			// Form URI
	when[256],			// When job queued/started/finished
      	hhmmss[64];			// Time HH:MM:SS


  snprintf(uri, sizeof(uri), "%s/jobs", job->printer->uriname);

  switch (papplJobGetState(job))
  {
    case IPP_JSTATE_PENDING :
	show_cancel = true;
        show_hold   = papplPrinterGetMaxActiveJobs(papplJobGetPrinter(job)) != 1;
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Queued %s"), time_string(client, papplJobGetTimeCreated(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_HELD :
	show_cancel  = true;
	show_release = true;
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Queued %s"), time_string(client, papplJobGetTimeCreated(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_PROCESSING :
    case IPP_JSTATE_STOPPED :
	if (papplJobIsCanceled(job))
	{
	  papplCopyString(when, papplClientGetLocString(client, _PAPPL_LOC("Canceling")), sizeof(when));
	}
	else
	{
	  show_cancel = true;
	  papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Started %s"), time_string(client, papplJobGetTimeProcessed(job), hhmmss, sizeof(hhmmss)));
	}
	break;

    case IPP_JSTATE_ABORTED :
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Aborted %s"), time_string(client, papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_CANCELED :
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Canceled %s"), time_string(client, papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;

    case IPP_JSTATE_COMPLETED :
	papplLocFormatString(papplClientGetLoc(client), when, sizeof(when), _PAPPL_LOC("Completed %s"), time_string(client, papplJobGetTimeCompleted(job), hhmmss, sizeof(hhmmss)));
	break;
  }

  papplClientHTMLPrintf(client, "              <tr><td>%d</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td><td>", papplJobGetID(job), papplJobGetName(job), papplJobGetUsername(job), papplJobGetImpressionsCompleted(job), when);

  if (show_cancel)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"cancel-job\"><input type=\"hidden\" name=\"job-id\" value=\"%d\"><input type=\"submit\" value=\"%s\"></form>", papplJobGetID(job), papplClientGetLocString(client, _PAPPL_LOC("Cancel Job")));
  }

  if (show_hold)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"hold-job\"><input type=\"hidden\" name=\"job-id\" value=\"%d\"><input type=\"submit\" value=\"%s\"></form>", papplJobGetID(job), papplClientGetLocString(client, _PAPPL_LOC("Hold Job")));
  }

  if (show_release)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"release-job\"><input type=\"hidden\" name=\"job-id\" value=\"%d\"><input type=\"submit\" value=\"%s\"></form>", papplJobGetID(job), papplClientGetLocString(client, _PAPPL_LOC("Release Job")));
  }

  if (papplJobGetState(job) >= IPP_JSTATE_ABORTED && job->filename)
  {
    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPrintf(client, "<input type=\"hidden\" name=\"action\" value=\"reprint-job\"><input type=\"hidden\" name=\"job-id\" value=\"%d\"><input type=\"submit\" value=\"%s\"></form>", papplJobGetID(job), papplClientGetLocString(client, _PAPPL_LOC("Reprint Job")));
  }

  papplClientHTMLPuts(client, "</td></tr>\n");
}


//
// 'job_pager()' - Show the job paging links.
//

static void
job_pager(pappl_client_t  *client,	// I - Client
	  pappl_printer_t *printer,	// I - Printer
	  int             job_index,	// I - First job shown (1-based)
	  int             limit)	// I - Maximum jobs shown
{
  int	num_jobs = 0,			// Number of jobs
	num_pages = 0,			// Number of pages
	i,				// Looping var
	page = 0;			// Current page
  char	path[1024];			// resource path


  if ((num_jobs = papplPrinterGetNumberOfJobs(printer)) <= limit)
    return;

  num_pages = (num_jobs + limit - 1) / limit;
  page      = (job_index - 1) / limit;

  snprintf(path, sizeof(path), "%s/jobs", printer->uriname);

  papplClientHTMLPuts(client, "          <div class=\"pager\">");

  if (page > 0)
    papplClientHTMLPrintf(client, "<a class=\"btn\" href=\"%s?job-index=%d\">&laquo;</a>", path, (page - 1) * limit + 1);

  for (i = 0; i < num_pages; i ++)
  {
    if (i == page)
      papplClientHTMLPrintf(client, " %d", i + 1);
    else
      papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s?job-index=%d\">%d</a>", path, i * limit + 1, i + 1);
  }

  if (page < (num_pages - 1))
    papplClientHTMLPrintf(client, " <a class=\"btn\" href=\"%s?job-index=%d\">&raquo;</a>", path, (page + 1) * limit + 1);

  papplClientHTMLPuts(client, "</div>\n");
}


//
// 'localize_keyword()' - Localize a media keyword...
//

static char *				// O - Localized string
localize_keyword(
    pappl_client_t *client,		// I - Client
    const char     *attrname,		// I - Attribute name
    const char     *keyword,		// I - Keyword string
    char           *buffer,		// I - String buffer
    size_t         bufsize)		// I - String buffer size
{
  char		pair[1024];		// attribute.keyword/enum pair
  const char	*locpair;		// Localized pair
  char		*ptr;			// Pointer into string


  // Try looking up the attribute.keyword/enum pair first...
  snprintf(pair, sizeof(pair), "%s.%s", attrname, keyword);
  locpair = papplClientGetLocString(client, pair);

  if (strcmp(pair, locpair))
  {
    // Have it, copy the localized string...
    papplCopyString(buffer, locpair, bufsize);
  }
  else if (!strcmp(attrname, "media"))
  {
    // Show dimensional media size...
    pwg_media_t *pwg = pwgMediaForPWG(keyword);
					// PWG media size info

    if ((pwg->width % 100) == 0 && (pwg->width % 2540) != 0)
      papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC(/*Media size in millimeters*/"%d x %dmm"), pwg->width / 100, pwg->length / 100);
    else
      papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC(/*Media size in inches*/"%g x %g\""), pwg->width / 2540.0, pwg->length / 2540.0);
  }
  else
  {
    // No localization, just capitalize the hyphenated words...
    papplCopyString(buffer, keyword, bufsize);
    *buffer = (char)toupper(*buffer & 255);
    for (ptr = buffer + 1; *ptr; ptr ++)
    {
      if (*ptr == '-' && ptr[1])
      {
	*ptr++ = ' ';
	*ptr   = (char)toupper(*ptr & 255);
      }
    }
  }

  return (buffer);
}


//
// 'localize_media()' - Localize media-col information.
//

static char *				// O - Localized description of the media
localize_media(
    pappl_client_t    *client,		// I - Client
    pappl_media_col_t *media,		// I - Media info
    bool              include_source,	// I - Include the media source?
    char              *buffer,		// I - String buffer
    size_t            bufsize)		// I - Size of string buffer
{
  char		size[128],		// Size name string
		source[128],		// Source string
		type[128];		// Type string
  const char	*borderless;		// Borderless qualifier


  if (!media->size_name[0])
    papplCopyString(size, papplClientGetLocString(client, _PAPPL_LOC("Unknown")), sizeof(size));
  else
    localize_keyword(client, "media", media->size_name, size, sizeof(size));

  if (!media->type[0])
    papplCopyString(type, papplClientGetLocString(client, _PAPPL_LOC("Unknown")), sizeof(type));
  else
    localize_keyword(client, "media-type", media->type, type, sizeof(type));

  if (!media->left_margin && !media->right_margin && !media->top_margin && !media->bottom_margin)
    borderless = papplClientGetLocString(client, _PAPPL_LOC(", Borderless"));
  else
    borderless = "";

  if (include_source)
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC(/*size (type+borderless) from source/tray*/"%s (%s%s) from %s"), size, type, borderless, localize_keyword(client, "media-source", media->source, source, sizeof(source)));
  else
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC(/*size (type+borderless)*/"%s (%s%s)"), size, type, borderless);

  return (buffer);
}


//
// 'media_chooser()' - Show the media chooser.
//

static void
media_chooser(
    pappl_client_t         *client,	// I - Client
    pappl_pr_driver_data_t *driver_data,// I - Driver data
    const char             *title,	// I - Label/title
    const char             *name,	// I - Base name
    pappl_media_col_t      *media)	// I - Current media values
{
  int		i,			// Looping var
		cur_index = 0,		// Current size index
	        sel_index = 0;		// Selected size index...
  pwg_media_t	*pwg;			// PWG media size info
  char		text[256];		// Human-readable value/text
  const char	*min_size = NULL,	// Minimum size
		*max_size = NULL;	// Maximum size


  // media-size
  papplLocFormatString(papplClientGetLoc(client), text, sizeof(text), _PAPPL_LOC(/*Source/Tray Media*/"%s Media"), title);
  papplClientHTMLPrintf(client, "              <tr><th>%s:</th><td>", text);
  for (i = 0; i < driver_data->num_media && (!min_size || !max_size); i ++)
  {
    if (!strncmp(driver_data->media[i], "custom_", 7) || !strncmp(driver_data->media[i], "roll_", 5))
    {
      if (strstr(driver_data->media[i], "_min_"))
        min_size = driver_data->media[i];
      else if (strstr(driver_data->media[i], "_max_"))
        max_size = driver_data->media[i];
    }
  }
  if (min_size && max_size)
  {
    papplClientHTMLPrintf(client, "<select name=\"%s-size\" onChange=\"show_hide_custom('%s');\"><option value=\"custom\">%s</option>", name, name, papplClientGetLocString(client, _PAPPL_LOC("Custom Size")));
    cur_index ++;
  }
  else
    papplClientHTMLPrintf(client, "<select name=\"%s-size\">", name);

  for (i = 0; i < driver_data->num_media; i ++)
  {
    if (!strncmp(driver_data->media[i], "custom_", 7) || !strncmp(driver_data->media[i], "roll_", 5))
    {
      if (strstr(driver_data->media[i], "_min_"))
        min_size = driver_data->media[i];
      else if (strstr(driver_data->media[i], "_max_"))
        max_size = driver_data->media[i];

      continue;
    }

    if (!strcmp(driver_data->media[i], media->size_name))
      sel_index = cur_index;

    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", driver_data->media[i], sel_index == cur_index ? " selected" : "", localize_keyword(client, "media", driver_data->media[i], text, sizeof(text)));
    cur_index ++;
  }
  if (min_size && max_size)
  {
    int cur_width, min_width, max_width;// Current/min/max width
    int cur_length, min_length, max_length;
					// Current/min/max length
    const char *cur_units;		// Current units

    if ((pwg = pwgMediaForPWG(min_size)) != NULL)
    {
      min_width  = pwg->width;
      min_length = pwg->length;
    }
    else
    {
      min_width  = 1 * 2540;
      min_length = 1 * 2540;
    }

    if ((pwg = pwgMediaForPWG(max_size)) != NULL)
    {
      max_width  = pwg->width;
      max_length = pwg->length;
    }
    else
    {
      max_width  = 9 * 2540;
      max_length = 22 * 2540;
    }

    if ((cur_width = media->size_width) < min_width)
      cur_width = min_width;
    else if (cur_width > max_width)
      cur_width = max_width;

    if ((cur_length = media->size_length) < min_length)
      cur_length = min_length;
    else if (cur_length > max_length)
      cur_length = max_length;

    if ((cur_units = media->size_name + strlen(media->size_name) - 2) < media->size_name)
      cur_units = "in";

    if (!strcmp(cur_units, "mm"))
      papplClientHTMLPrintf(client, "</select><div style=\"display: %s;\" id=\"%s-custom\"><input type=\"number\" name=\"%s-custom-width\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"%s\">x<input type=\"number\" name=\"%s-custom-length\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"%s\"><div class=\"switch\"><input type=\"radio\" id=\"%s-custom-units-in\" name=\"%s-custom-units\" value=\"in\"><label for=\"%s-custom-units-in\">in</label><input type=\"radio\" id=\"%s-custom-units-mm\" name=\"%s-custom-units\" value=\"mm\" checked><label for=\"%s-custom-units-mm\">mm</label></div></div>\n", sel_index == 0 ? "inline-block" : "none", name, name, min_width / 2540.0, max_width / 100.0, cur_width / 100.0, papplClientGetLocString(client, _PAPPL_LOC("Width")), name, min_length / 2540.0, max_length / 100.0, cur_length / 100.0, papplClientGetLocString(client, _PAPPL_LOC("Height")), name, name, name, name, name, name);
    else
      papplClientHTMLPrintf(client, "</select><div style=\"display: %s;\" id=\"%s-custom\"><input type=\"number\" name=\"%s-custom-width\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"%s\">x<input type=\"number\" name=\"%s-custom-length\" min=\"%.2f\" max=\"%.2f\" value=\"%.2f\" step=\".01\" placeholder=\"%s\"><div class=\"switch\"><input type=\"radio\" id=\"%s-custom-units-in\" name=\"%s-custom-units\" value=\"in\" checked><label for=\"%s-custom-units-in\">in</label><input type=\"radio\" id=\"%s-custom-units-mm\" name=\"%s-custom-units\" value=\"mm\"><label for=\"%s-custom-units-mm\">mm</label></div></div>\n", sel_index == 0 ? "inline-block" : "none", name, name, min_width / 2540.0, max_width / 100.0, cur_width / 2540.0, papplClientGetLocString(client, _PAPPL_LOC("Width")), name, min_length / 2540.0, max_length / 100.0, cur_length / 2540.0, papplClientGetLocString(client, _PAPPL_LOC("Height")), name, name, name, name, name, name);
  }
  else
    papplClientHTMLPuts(client, "</select>\n");

  if (driver_data->borderless)
  {
    papplClientHTMLPrintf(client, "                <input type=\"checkbox\" name=\"%s-borderless\"%s>&nbsp;%s\n", name, (!media->bottom_margin && !media->left_margin && !media->right_margin && !media->top_margin) ? " checked" : "", papplClientGetLocString(client, _PAPPL_LOC("Borderless")));
  }

  // media-left/top-offset (if needed)
  if (driver_data->left_offset_supported[1] || driver_data->top_offset_supported[1])
  {
    papplClientHTMLPrintf(client, "                %s&nbsp;", papplClientGetLocString(client, _PAPPL_LOC("Offset")));

    if (driver_data->left_offset_supported[1])
    {
      papplClientHTMLPrintf(client, "<input type=\"number\" name=\"%s-left-offset\" min=\"%.1f\" max=\"%.1f\" step=\"0.1\" value=\"%.1f\">", name, driver_data->left_offset_supported[0] / 100.0, driver_data->left_offset_supported[1] / 100.0, media->left_offset / 100.0);

      if (driver_data->top_offset_supported[1])
        papplClientHTMLPuts(client, "&nbsp;x&nbsp;");
    }

    if (driver_data->top_offset_supported[1])
      papplClientHTMLPrintf(client, "<input type=\"number\" name=\"%s-top-offset\" min=\"%.1f\" max=\"%.1f\" step=\"0.1\" value=\"%.1f\">", name, driver_data->top_offset_supported[0] / 100.0, driver_data->top_offset_supported[1] / 100.0, media->top_offset / 100.0);

    papplClientHTMLPuts(client, "&nbsp;mm\n");
  }

  // media-tracking (if needed)
  if (driver_data->tracking_supported)
  {
    papplClientHTMLPrintf(client, "                <select name=\"%s-tracking\">", name);
    for (i = PAPPL_MEDIA_TRACKING_CONTINUOUS; i <= PAPPL_MEDIA_TRACKING_WEB; i *= 2)
    {
      const char *val = _papplMediaTrackingString((pappl_media_tracking_t)i);

      if (!(driver_data->tracking_supported & i))
	continue;

      papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", val, (pappl_media_tracking_t)i == media->tracking ? " selected" : "", localize_keyword(client, "media-tracking", val, text, sizeof(text)));
    }
    papplClientHTMLPuts(client, "</select>\n");
  }

  // media-type
  papplClientHTMLPrintf(client, "                <select name=\"%s-type\">", name);
  for (i = 0; i < driver_data->num_type; i ++)
  {
    papplClientHTMLPrintf(client, "<option value=\"%s\"%s>%s</option>", driver_data->type[i], !strcmp(driver_data->type[i], media->type) ? " selected" : "", localize_keyword(client, "media-type", driver_data->type[i], text, sizeof(text)));
  }
  papplClientHTMLPrintf(client, "</select></td></tr>\n");
}


//
// 'time_string()' - Return the local time in hours, minutes, and seconds.
//

static char *				// O - Formatted time string
time_string(pappl_client_t *client,	// I - Client
            time_t         tv,		// I - Time value
            char           *buffer,	// I - Buffer
	    size_t         bufsize)	// I - Size of buffer
{
  struct tm	date;			// Local time and date
  time_t	age;			// How old is the time?


  // Get the local time in hours, minutes, and seconds...
  localtime_r(&tv, &date);

  // See how long ago this was...
  age = time(NULL) - tv;

  // Format based on the age...
  if (age < 86400)
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC("at %02d:%02d:%02d"), date.tm_hour, date.tm_min, date.tm_sec);
  else if (age < (2 * 86400))
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC("yesterday at %02d:%02d:%02d"), date.tm_hour, date.tm_min, date.tm_sec);
  else if (age < (31 * 86400))
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC("%d days ago at %02d:%02d:%02d"), (int)(age / 86400), date.tm_hour, date.tm_min, date.tm_sec);
  else
    papplLocFormatString(papplClientGetLoc(client), buffer, bufsize, _PAPPL_LOC("%04d-%02d-%02d at %02d:%02d:%02d"), date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);

  // Return the formatted string...
  return (buffer);
}
