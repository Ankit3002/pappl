//
// Printer object for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"
#include <stdio.h>


//
// Local functions...
//

static int	compare_active_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_all_jobs(pappl_job_t *a, pappl_job_t *b);
static int	compare_completed_jobs(pappl_job_t *a, pappl_job_t *b);


//
// 'papplPrinterCancelAllJobs()' - Cancel all jobs on the printer.
//
// This function cancels all jobs on the printer.  If any job is currently being
// printed, it will be stopped at a convenient time (usually the end of a page)
// so that the printer will be left in a known state.
//

void
papplPrinterCancelAllJobs(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_job_t	*job;			// Job information


  // Loop through all jobs and cancel them.
  //
  // Since we have a writer lock, it is safe to use cupsArrayGetFirst/Last...
  _papplRWLockWrite(printer);

  for (job = (pappl_job_t *)cupsArrayGetFirst(printer->active_jobs); job; job = (pappl_job_t *)cupsArrayGetNext(printer->active_jobs))
  {
    // Cancel this job...
    if (job->state == IPP_JSTATE_PROCESSING || (job->state == IPP_JSTATE_HELD && job->fd >= 0))
    {
      job->is_canceled = true;
    }
    else
    {
      job->state     = IPP_JSTATE_CANCELED;
      job->completed = time(NULL);

      _papplJobRemoveFile(job);

      cupsArrayRemove(printer->active_jobs, job);
      cupsArrayAdd(printer->completed_jobs, job);
    }
  }

  _papplRWUnlock(printer);

  if (!printer->system->clean_time)
    printer->system->clean_time = time(NULL) + 60;
}

// api for creating preset in the printer ...
static char *				// O  - Line or `NULL` on EOF
read_line(cups_file_t *fp,		// I  - File
          char        *line,		// I  - Line buffer
          size_t      linesize,		// I  - Size of line buffer
          char        **value,		// O  - Value portion of line
          int         *linenum)		// IO - Current line number
{
  char	*ptr;				// Pointer into line


  // Try reading a line from the file...
  *value = NULL;

  if (!cupsFileGets(fp, line, linesize))
    return (NULL);

  // Got it, bump the line number...
  (*linenum) ++;

  // If we have "something value" then split at the whitespace...
  if ((ptr = strchr(line, ' ')) != NULL)
  {
    *ptr++ = '\0';
    *value = ptr;
  }

  // Strip the trailing ">" for "<something value(s)>"
  if (line[0] == '<' && *value && (ptr = *value + strlen(*value) - 1) >= *value && *ptr == '>')
    *ptr = '\0';

  return (line);
}

// this api is used to convert file descriptor 
// into cups_file_t descriptor...

cups_file_t *
papplFileOpenFd(
              const char *mode,
              int fd)
{
  cups_file_t *fp;
  if ((fp = cupsFileOpenFd(fd, mode)) == NULL)
  {
    if (*mode == 's')
      httpAddrClose(NULL, fd);
    else
      close(fd);
  }
  return (fp);
}

// this api is used to add preset of a particular printer ...


// void
// papplPresetAdd(pappl_system_t *system , pappl_printer_t * printer )
// {

//       /// for doing printer validation ...
//       const char	*printer_id,	// Printer ID
// 			*printer_name,	// Printer name
// 			*device_id,	// Device ID
// 			*device_uri,	// Device URI
// 			*driver_name;	// Driver name


//       //variables ..
//       int i;
//       int fp;
//       int linenum;
//       char line[2048];
//       char *ptr;
//       char *value;

//   //create file descriptor for cups_file_t.....
//   cups_file_t *fd;

//   char filename[1024];
   
//   fp = papplPrinterOpenFile(printer,filename, sizeof(filename), "/home/ankit/Documents", "preset_option", "txt", "r");
//   linenum = 0;

//   fd = papplFileOpenFd("r", fp);

//   printf("\nthe value of file descriptor is --- %d\n", fp);
  

//   while(read_line(fd, line , sizeof(line), &value, &linenum))
//   {

//     if(!strcasecmp(line , "<Printer") && value)
//     {

//       cups_len_t	num_options;	// Number of options
//       cups_option_t	*options = NULL;// Options

//       if ((num_options = cupsParseOptions(value, 0, &options)) != 5 || (printer_id = cupsGetOption("id", num_options, options)) == NULL || strtol(printer_id, NULL, 10) <= 0 || (printer_name = cupsGetOption("name", num_options, options)) == NULL || (device_id = cupsGetOption("did", num_options, options)) == NULL || (device_uri = cupsGetOption("uri", num_options, options)) == NULL || (driver_name = cupsGetOption("driver", num_options, options)) == NULL)
//       {
//         printf("The definition written in the file for printer tag is incorrect\n");
//       }

//       // // you got those values ...
//       if(!strcasecmp(printer_name, printer->name) 
//         && !strcasecmp(device_uri, printer->device_uri)
//         && !strcasecmp(driver_name, printer->driver_name))
//         {

//           printf("--------------------you get the right printer buddy---------*******-------\n");

            
//           while (read_line(fd, line, sizeof(line), &value, &linenum))
//           {
            
//             if(!strcasecmp(line , "<Preset") && value)
//             {
//               // Read a preset ...
//               const char *preset_name,
//               *preset_id;

//               pappl_pr_preset_data_t *preset; // current preset
//                 // Allocate memory for the printer...
//               if ((preset = calloc(1, sizeof(pappl_pr_preset_data_t))) == NULL)
//               {
//                 printf("Allocate memory for preset fails ... \n");
//               }
                      
//               if ((num_options = cupsParseOptions(value, 0, &options)) != 2 || (preset_id= cupsGetOption("id", num_options, options)) == NULL || strtol(preset_id, NULL, 10) <= 0 || (preset_name = cupsGetOption("name", num_options, options)) == NULL )
//               {
//                 printf("the preset definition is incorrect \n");
//                 // papplLog(system, PAPPL_LOGLEVEL_ERROR, "Bad preset definition on line %d of '%s'.", linenum, filename);
//                 break;
//               }

//               preset->name = preset_name;
              
//               /*
//                * All the properties of printer get read from the below while loop...
//               */

//               while (read_line(fd, line, sizeof(line), &value, &linenum))
//               {
//                 if(!strcasecmp(line, "</Preset"))
//                   break;

//                   // here set the values of preset to printer object ...
//                   else if (!strcasecmp(line, "identify-actions-default"))
//                     preset->identify_default = _papplIdentifyActionsValue(value);
//                   else if (!strcasecmp(line, "label-mode-configured"))
//                     preset->mode_configured = _papplLabelModeValue(value);
//                   else if (!strcasecmp(line, "label-tear-offset-configured") && value)
//                     preset->tear_offset_configured = (int)strtol(value, NULL, 10);
//                   else if (!strcasecmp(line, "media-col-default"))
//                     parse_media_col(value, &preset->media_default);
//                   else if (!strncasecmp(line, "media-col-ready", 15))
//                   {
//                     if ((i = (int)strtol(line + 15, NULL, 10)) >= 0 && i < PAPPL_MAX_SOURCE)
//                       parse_media_col(value, preset->media_ready + i);
//                   }
//                   else if (!strcasecmp(line, "orientation-requested-default"))
//                     preset->orient_default = (ipp_orient_t)ippEnumValue("orientation-requested", value);
//                   else if (!strcasecmp(line, "output-bin-default") && value)
//                   {
//                     for (i = 0; i < preset->num_bin; i ++)
//                     {
//                       if (!strcmp(value, preset->bin[i]))
//                       {
//                         preset->bin_default = i;
//                         break;
//                       }
//                     }
//                   }
//                   else if (!strcasecmp(line, "print-color-mode-default"))
//                     preset->color_default = _papplColorModeValue(value);
//                   else if (!strcasecmp(line, "print-content-optimize-default"))
//                     preset->content_default = _papplContentValue(value);
//                   else if (!strcasecmp(line, "print-darkness-default") && value)
//                     preset->darkness_default = (int)strtol(value, NULL, 10);
//                   else if (!strcasecmp(line, "print-quality-default"))
//                     preset->quality_default = (ipp_quality_t)ippEnumValue("print-quality", value);
//                   else if (!strcasecmp(line, "print-scaling-default"))
//                     preset->scaling_default = _papplScalingValue(value);
//                   else if (!strcasecmp(line, "print-speed-default") && value)
//                     preset->speed_default = (int)strtol(value, NULL, 10);
//                   else if (!strcasecmp(line, "printer-darkness-configured") && value)
//                     preset->darkness_configured = (int)strtol(value, NULL, 10);
//                   else if (!strcasecmp(line, "printer-resolution-default") && value)
//                     sscanf(value, "%dx%ddpi", &preset->x_default, &preset->y_default);
//                   else if (!strcasecmp(line, "sides-default"))
//                     preset->sides_default = _papplSidesValue(value);


//                   // now set vendor attributes ...
//                   else if ((ptr = strstr(line, "-default")) != NULL)
//                   {
//                     char	defname[128],		// xxx-default name
//                     supname[128];		// xxx-supported name
//               ipp_attribute_t *attr;	// Attribute

//                     *ptr = '\0';

//                     snprintf(defname, sizeof(defname), "%s-default", line);
//                     snprintf(supname, sizeof(supname), "%s-supported", line);

//                     if (!value)
//                       value = ptr;

//               ippDeleteAttribute(preset->vendor_attrs, ippFindAttribute(preset->vendor_attrs, defname, IPP_TAG_ZERO));

//                     if ((attr = ippFindAttribute(preset->vendor_attrs, supname, IPP_TAG_ZERO)) != NULL)
//                     {
//                       switch (ippGetValueTag(attr))
//                       {
//                         case IPP_TAG_BOOLEAN :
//                             ippAddBoolean(preset->vendor_attrs, IPP_TAG_PRINTER, defname, !strcmp(value, "true"));
//                             break;

//                         case IPP_TAG_INTEGER :
//                         case IPP_TAG_RANGE :
//                             ippAddInteger(preset->vendor_attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, defname, (int)strtol(value, NULL, 10));
//                             break;

//                         case IPP_TAG_KEYWORD :
//                 ippAddString(preset->vendor_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, defname, NULL, value);
//                             break;

//                         default :
//                             break;
//                       }
//               }
//                     else
//                     {
//                       ippAddString(preset->vendor_attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, defname, NULL, value);
//                     }
//                   }

          
//               }

//               // now you have the preset ... 
//               // call the method which will 

//                 // Add the preset to the printer...

//                   printf("****************** -------------the number of presets created are --- %d\n", cupsArrayCount(printer->presets));

//                 _papplSystemAddPreset(system , printer, preset);

//                   printf("****************** -------------the number of presets created are --- %d\n", cupsArrayCount(printer->presets));


              
//             }
//           }
        


//         }
//         else
//         {
//           printf("No chance !\n");
//         }


//     }
//     linenum++;
//     if(!strcasecmp(line, "</Printer>"))
//     {
//       break;
//     }

//   }
  

//   close(fp);

// }


// work new api over here...



void
papplPresetAdd(pappl_system_t *system , pappl_printer_t * printer )
{


  printf(" \n \n THE PAPPLPRESETADD IS CALLED \n");

      /// for doing printer validation ...

      //variables ..
      int i;
      int fp;
      int linenum;
      char line[2048];
      char *ptr;
      char *value;

  //create file descriptor for cups_file_t.....
  cups_file_t *fd;

  char filename[1024];
   
  
  fp =  papplPrinterOpenFile(printer,filename, sizeof(filename), system->directory, "preset_option", "txt", "r");
  
  linenum = 0;



  fd = papplFileOpenFd("r", fp);

  printf("\nthe value of file descriptor is --- %d\n", fp);



  // reading the file ...

  while(read_line(fd, line , sizeof(line), &value, &linenum))
  {

    printf("THE LINE ---> %s  VALUE --> %s \n", line , value);

      cups_len_t	num_options;	// Number of options
      cups_option_t	*options = NULL;// Options


      if(!strcasecmp(line , "<Preset") && value)
      {
        // Read a preset ...
        const char *preset_name,
        *preset_id;

        pappl_pr_preset_data_t *preset; // current preset
        
        
          // Allocate memory for the Preset...
        if ((preset = calloc(1, sizeof(pappl_pr_preset_data_t))) == NULL)
        {
          printf("Allocate memory for preset fails ... \n");
        }

        // allocate memeory for driver attribues in the preset...
        preset->driver_attrs = ippNew();
                
        if ((num_options = cupsParseOptions(value, 0, &options)) != 2 || (preset_id= cupsGetOption("id", num_options, options)) == NULL || strtol(preset_id, NULL, 10) <= 0 || (preset_name = cupsGetOption("name", num_options, options)) == NULL )
        {
          printf("the preset definition is incorrect \n");
          // papplLog(system, PAPPL_LOGLEVEL_ERROR, "Bad preset definition on line %d of '%s'.", linenum, filename);
          break;
        }

        // Assign the name, id of the preset ...
        preset->name  = strdup(preset_name);
        preset->preset_id = strtol(preset_id, NULL, 10);

        /*
          * All the properties of printer get read from the below while loop...
        */

        while (read_line(fd, line, sizeof(line), &value, &linenum))
        {
          if(!strcasecmp(line, "</Preset>"))
          {

          _papplSystemAddPreset(system , printer, preset);
            break;
          }
            

            // here set the values of preset to printer object ...
            else if (!strcasecmp(line, "identify-actions-default"))
              preset->identify_default = _papplIdentifyActionsValue(value);
            else if (!strcasecmp(line, "label-mode-configured"))
              preset->mode_configured = _papplLabelModeValue(value);
            else if (!strcasecmp(line, "label-tear-offset-configured") && value)
              preset->tear_offset_configured = (int)strtol(value, NULL, 10);
            else if (!strcasecmp(line, "media-col-default"))
              parse_media_col(value, &preset->media_default);
            else if (!strncasecmp(line, "media-col-ready", 15))
            {
              if ((i = (int)strtol(line + 15, NULL, 10)) >= 0 && i < PAPPL_MAX_SOURCE)
                parse_media_col(value, preset->media_ready + i);
            }
            else if (!strcasecmp(line, "orientation-requested-default"))
              preset->orient_default = (ipp_orient_t)ippEnumValue("orientation-requested", value);
            else if (!strcasecmp(line, "output-bin-default") && value)
            {
              for (i = 0; i < preset->num_bin; i ++)
              {
                if (!strcmp(value, preset->bin[i]))
                {
                  preset->bin_default = i;
                  break;
                }
              }
            }
            else if (!strcasecmp(line, "print-color-mode-default"))
              preset->color_default = _papplColorModeValue(value);
            else if (!strcasecmp(line, "print-content-optimize-default"))
              preset->content_default = _papplContentValue(value);
            else if (!strcasecmp(line, "print-darkness-default") && value)
              preset->darkness_default = (int)strtol(value, NULL, 10);
            else if (!strcasecmp(line, "print-quality-default"))
              preset->quality_default = (ipp_quality_t)ippEnumValue("print-quality", value);
            else if (!strcasecmp(line, "print-scaling-default"))
              preset->scaling_default = _papplScalingValue(value);
            else if (!strcasecmp(line, "print-speed-default") && value)
              preset->speed_default = (int)strtol(value, NULL, 10);
            else if (!strcasecmp(line, "printer-darkness-configured") && value)
              preset->darkness_configured = (int)strtol(value, NULL, 10);
            else if (!strcasecmp(line, "printer-resolution-default") && value)
              sscanf(value, "%dx%ddpi", &preset->x_default, &preset->y_default);
            else if (!strcasecmp(line, "sides-default"))
              preset->sides_default = _papplSidesValue(value);


            // // now set vendor attributes ...
            else if ((ptr = strstr(line, "-default")) != NULL)
            {
              // printf("the vendor options are called --- %s\n", line);
              char	defname[128],		// xxx-default name
              supname[128];		// xxx-supported name
              ipp_attribute_t *attr;	// Attribute

              *ptr = '\0';

              snprintf(defname, sizeof(defname), "%s-default", line);
              snprintf(supname, sizeof(supname), "%s-supported", line);

              
              
              // these are written in the file ... the preset one and the legacy file ...

              // the below will check whether we have some value in value variable .... if not then assgin it with line null pointer
              if (!value)
              {
                    value = ptr;
                    printf("the value of value is -- %s\n", value);

              }
                
              

              // We delete the particular value in preset --- because we want to overrite in the structure with that ... memory overwritten check ...

              ippDeleteAttribute(preset->driver_attrs, ippFindAttribute(preset->driver_attrs, defname, IPP_TAG_ZERO));




              // when we won't be able to find presets in the printer ... itself ..
              if ((attr = ippFindAttribute(printer->driver_attrs, supname, IPP_TAG_ZERO)) != NULL)
              {
                
                // printf("FOR  supname --- %s the the printer contains its declaration ..\n  ", supname);
                //  printf("  \n");

                switch (ippGetValueTag(attr))
                {
                  case IPP_TAG_BOOLEAN :
                      printf("So the IPP_TAG_BOOLEAN is called ...\n");
                      printf("    \n");
                      ippAddBoolean(preset->driver_attrs, IPP_TAG_PRINTER, defname, !strcmp(value, "true"));
                      break;

                  case IPP_TAG_INTEGER :
                  case IPP_TAG_RANGE :
                      ippAddInteger(preset->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, defname, (int)strtol(value, NULL, 10));
                        printf("So the IPP_TAG_INTEGER AND RANGE is called ...\n");
                      printf("    \n");
                      break;

                  case IPP_TAG_KEYWORD :
                      ippAddString(preset->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, defname, NULL, value);
                        printf("So the IPP_TAG_KEYWORD is called ...\n");
                      printf("    \n");
                                  break;

                  default :
                      break;
                }
              }
              else
              {
                // printf("The else part is working in adding the attribute to preset object ...\n");
                ippAddString(preset->driver_attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, defname, NULL, value);

              }
            }

    
        }
      }
  }
  
  cupsFileClose(fd);

  close(fp);

}




//
// 'parse_media_col()' - Parse a media-col value.
//

 void
parse_media_col(
    char              *value,		// I - Value
    pappl_media_col_t *media)		// O - Media collection
{
  cups_len_t	i,			// Looping var
		num_options;		// Number of options
  cups_option_t	*options = NULL,	// Options
		*option;		// Current option


  memset(media, 0, sizeof(pappl_media_col_t));
  num_options = cupsParseOptions(value, 0, &options);

  for (i = num_options, option = options; i > 0; i --, option ++)
  {
    if (!strcasecmp(option->name, "bottom"))
      media->bottom_margin = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "left"))
      media->left_margin = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "left-offset"))
      media->left_offset = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "right"))
      media->right_margin = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "name"))
      papplCopyString(media->size_name, option->value, sizeof(media->size_name));
    else if (!strcasecmp(option->name, "width"))
      media->size_width = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "length"))
      media->size_length = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "source"))
      papplCopyString(media->source, option->value, sizeof(media->source));
    else if (!strcasecmp(option->name, "top"))
      media->top_margin = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "offset") || !strcasecmp(option->name, "top-offset"))
      media->top_offset = (int)strtol(option->value, NULL, 10);
    else if (!strcasecmp(option->name, "tracking"))
      media->tracking = _papplMediaTrackingValue(option->value);
    else if (!strcasecmp(option->name, "type"))
      papplCopyString(media->type, option->value, sizeof(media->type));
  }

  cupsFreeOptions(num_options, options);
}

// Api for adding preset object into the printer object of system object ...

void _papplSystemAddPreset(
    pappl_system_t          *system,    // I - System
    pappl_printer_t         *printer,   // I - Printer
    pappl_pr_preset_data_t  *preset)    // I - Preset
{
    // Add the preset to the printer...
    _papplRWLockWrite(system);

    if (!printer->presets)
        printer->presets = cupsArrayNew3(NULL, NULL, NULL, 0, NULL, NULL);
    

 
    cupsArrayAdd(printer->presets, preset);

    _papplRWUnlock(system);
       printf("Checking whether this is working or not .... \n");
    _papplSystemConfigChanged(system);
    // papplSystemAddEvent(system, printer, NULL, PAPPL_EVENT_PRINTER_PRESET_CREATED | PAPPL_EVENT_SYSTEM_CONFIG_CHANGED, NULL);
}





//
// 'papplPrinterCreate()' - Create a new printer.
//
// This function creates a new printer (service) on the specified system.  The
// "type" argument specifies the type of service to create and must currently
// be the value `PAPPL_SERVICE_TYPE_PRINT`.
//
// The "printer_id" argument specifies a positive integer identifier that is
// unique to the system.  If you specify a value of `0` a new identifier will
// be assigned.
//
// The "driver_name" argument specifies a named driver for the printer, from
// the list of drivers registered with the @link papplSystemSetPrinterDrivers@
// function.
//
// The "device_id" and "device_uri" arguments specify the IEEE-1284 device ID
// and device URI strings for the printer.
//
// On error, this function sets the `errno` variable to one of the following
// values:
//
// - `EEXIST`: A printer with the specified name already exists.
// - `EINVAL`: Bad values for the arguments were specified.
// - `EIO`: The driver callback failed.
// - `ENOENT`: No driver callback has been set.
// - `ENOMEM`: Ran out of memory.
//


pappl_printer_t *			// O - Printer or `NULL` on error
papplPrinterCreate(
    pappl_system_t       *system,	// I - System
    int                  printer_id,	// I - printer-id value or `0` for new
    const char           *printer_name,	// I - Human-readable printer name
    const char           *driver_name,	// I - Driver name
    const char           *device_id,	// I - IEEE-1284 device ID
    const char           *device_uri)	// I - Device URI
{
  printf("papplPrinterCreate is called to create the printer in the system object .. \n");
  pappl_printer_t	*printer;	// Printer
  char			resource[1024],	// Resource path
			*resptr,	// Pointer into resource path
			uuid[128],	// printer-uuid
			print_group[65];// print-group value
  int			k_supported;	// Maximum file size supported
#if !_WIN32
  struct statfs		spoolinfo;	// FS info for spool directory
  double		spoolsize;	// FS size
#endif // !_WIN32
  char			path[256];	// Path to resource
  pappl_pr_driver_data_t driver_data;	// Driver data
  ipp_t			*driver_attrs;	// Driver attributes
  static const char * const ipp_versions[] =
  {					// ipp-versions-supported values
    "1.1",
    "2.0"
  };
  static const int	operations[] =	// operations-supported values
  {
    IPP_OP_PRINT_JOB,
    IPP_OP_VALIDATE_JOB,
    IPP_OP_CREATE_JOB,
    IPP_OP_SEND_DOCUMENT,
    IPP_OP_CANCEL_JOB,
    IPP_OP_GET_JOB_ATTRIBUTES,
    IPP_OP_GET_JOBS,
    IPP_OP_GET_PRINTER_ATTRIBUTES,   // this will add the operation to fetch the printer attributes ...
    IPP_OP_PAUSE_PRINTER,
    IPP_OP_RESUME_PRINTER,
    IPP_OP_SET_PRINTER_ATTRIBUTES,
    IPP_OP_GET_PRINTER_SUPPORTED_VALUES,
    IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS,
    IPP_OP_CREATE_JOB_SUBSCRIPTIONS,
    IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES,
    IPP_OP_GET_SUBSCRIPTIONS,
    IPP_OP_RENEW_SUBSCRIPTION,
    IPP_OP_CANCEL_SUBSCRIPTION,
    IPP_OP_GET_NOTIFICATIONS,
    IPP_OP_ENABLE_PRINTER,
    IPP_OP_DISABLE_PRINTER,
    IPP_OP_PAUSE_PRINTER_AFTER_CURRENT_JOB,
    IPP_OP_CANCEL_CURRENT_JOB,
    IPP_OP_CANCEL_JOBS,
    IPP_OP_CANCEL_MY_JOBS,
    IPP_OP_CLOSE_JOB,
    IPP_OP_IDENTIFY_PRINTER,
    IPP_OP_HOLD_JOB,
    IPP_OP_RELEASE_JOB,
    IPP_OP_HOLD_NEW_JOBS,
    IPP_OP_RELEASE_HELD_NEW_JOBS
  };
  static const char * const charset[] =	// charset-supported values
  {
    "us-ascii",
    "utf-8"
  };
  static const char * const client_info[] =
  {					// client-info-supported values
    "client-name",
    "client-patches",
    "client-string-version",
    "client-version"
  };
  static const char * const compression[] =
  {					// compression-supported values
    "deflate",
    "gzip",
    "none"
  };
  static const char * const job_hold_until[] =
  {					// job-hold-until-supported values
    "day-time",
    "evening",
    "indefinite",
    "night",
    "no-hold",
    "second-shift",
    "third-shift",
    "weekend"
  };
  static const char * const multiple_document_handling[] =
  {					// multiple-document-handling-supported values
    "separate-documents-uncollated-copies",
    "separate-documents-collated-copies"
  };
  static const int orientation_requested[] =
  {
    IPP_ORIENT_PORTRAIT,
    IPP_ORIENT_LANDSCAPE,
    IPP_ORIENT_REVERSE_LANDSCAPE,
    IPP_ORIENT_REVERSE_PORTRAIT,
    IPP_ORIENT_NONE
  };
  static const char * const print_content_optimize[] =
  {					// print-content-optimize-supported
    "auto",
    "graphic",
    "photo",
    "text-and-graphic",
    "text"
  };
  static const char *print_processing[] =
  {					// print-processing-attributes-supported
    "print-color-mode",
    "printer-resolution"
  };
  static const int print_quality[] =	// print-quality-supported
  {
    IPP_QUALITY_DRAFT,
    IPP_QUALITY_NORMAL,
    IPP_QUALITY_HIGH
  };
  static const char * const print_scaling[] =
  {					// print-scaling-supported
    "auto",
    "auto-fit",
    "fill",
    "fit",
    "none"
  };
  static const char * const uri_security[] =
  {					// uri-security-supported values
    "none",
    "tls"
  };
  static const char * const which_jobs[] =
  {					// which-jobs-supported values
    "completed",
    "not-completed",
    "all"
  };


  // Range check input...
  if (!system || !printer_name || !driver_name || !device_uri)
  {
    errno = EINVAL;
    return (NULL);
  }

  if (!system->driver_cb)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No driver callback set, unable to add printer.");
    errno = ENOENT;
    return (NULL);
  }

  // Prepare URI values for the printer attributes...
  if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
  {
    // Make sure printer names that start with a digit have a resource path
    // containing an underscore...
    if (isdigit(*printer_name & 255))
      snprintf(resource, sizeof(resource), "/ipp/print/_%s", printer_name);
    else
      snprintf(resource, sizeof(resource), "/ipp/print/%s", printer_name);

    // Convert URL reserved characters to underscore...
    for (resptr = resource + 11; *resptr; resptr ++)
    {
      if ((*resptr & 255) <= ' ' || strchr("\177/\\\'\"?#", *resptr))
	*resptr = '_';
    }

    // Eliminate duplicate and trailing underscores...
    resptr = resource + 11;
    while (*resptr)
    {
      if (resptr[0] == '_' && resptr[1] == '_')
        memmove(resptr, resptr + 1, strlen(resptr));
					// Duplicate underscores
      else if (resptr[0] == '_' && !resptr[1])
        *resptr = '\0';			// Trailing underscore
      else
        resptr ++;
    }
  }
  else
    papplCopyString(resource, "/ipp/print", sizeof(resource));

  // Make sure the printer doesn't already exist...
  if ((printer = papplSystemFindPrinter(system, resource, 0, NULL)) != NULL)
  {
    int		n;		// Current instance number
    char	temp[1024];	// Temporary resource path

    if (!strcmp(printer_name, printer->name))
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Printer '%s' already exists.", printer_name);
      errno = EEXIST;
      return (NULL);
    }

    for (n = 2; n < 10; n ++)
    {
      snprintf(temp, sizeof(temp), "%s_%d", resource, n);
      if (!papplSystemFindPrinter(system, temp, 0, NULL))
        break;
    }

    if (n >= 10)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR, "Printer '%s' name conflicts with existing printer.", printer_name);
      errno = EEXIST;
      return (NULL);
    }

    papplCopyString(resource, temp, sizeof(resource));
  }

  // Allocate memory for the printer...
  if ((printer = calloc(1, sizeof(pappl_printer_t))) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for printer: %s", strerror(errno));
    return (NULL);
  }

  papplLog(system, PAPPL_LOGLEVEL_INFO, "Printer '%s' at resource path '%s'.", printer_name, resource);

  _papplSystemMakeUUID(system, printer_name, 0, uuid, sizeof(uuid));

  // Get the maximum spool size based on the size of the filesystem used for
  // the spool directory.  If the host OS doesn't support the statfs call
  // or the filesystem is larger than 2TiB, always report INT_MAX.
#if _WIN32
  k_supported = INT_MAX;
#else // !_WIN32
  if (statfs(system->directory, &spoolinfo))
    k_supported = INT_MAX;
  else if ((spoolsize = (double)spoolinfo.f_bsize * spoolinfo.f_blocks / 1024) > INT_MAX)
    k_supported = INT_MAX;
  else
    k_supported = (int)spoolsize;
#endif // _WIN32

  // Initialize printer structure and attributes...
  pthread_rwlock_init(&printer->rwlock, NULL);

  printer->system             = system;
  printer->name               = strdup(printer_name);
  printer->dns_sd_name        = strdup(printer_name);
  printer->resource           = strdup(resource);
  printer->resourcelen        = strlen(resource);
  printer->uriname            = printer->resource + 10; // Skip "/ipp/print" in resource
  printer->device_id          = device_id ? strdup(device_id) : NULL;
  printer->device_uri         = strdup(device_uri);
  printer->driver_name        = strdup(driver_name);
  printer->attrs              = ippNew();
  printer->start_time         = time(NULL);
  printer->config_time        = printer->start_time;
  printer->state              = IPP_PSTATE_IDLE;
  printer->state_reasons      = PAPPL_PREASON_NONE;
  printer->state_time         = printer->start_time;
  printer->is_accepting       = true;
  printer->all_jobs           = cupsArrayNew((cups_array_cb_t)compare_all_jobs, NULL, NULL, 0, NULL, (cups_afree_cb_t)_papplJobDelete);
  printer->active_jobs        = cupsArrayNew((cups_array_cb_t)compare_active_jobs, NULL, NULL, 0, NULL, NULL);
  printer->completed_jobs     = cupsArrayNew((cups_array_cb_t)compare_completed_jobs, NULL, NULL, 0, NULL, NULL);
  printer->next_job_id        = 1;
  printer->max_active_jobs    = (system->options & PAPPL_SOPTIONS_MULTI_QUEUE) ? 0 : 1;
  printer->max_completed_jobs = 100;
  printer->usb_vendor_id      = 0x1209;	// See <https://pid.codes>
  printer->usb_product_id     = 0x8011;

  if (!printer->name || !printer->dns_sd_name || !printer->resource || (device_id && !printer->device_id) || !printer->device_uri || !printer->driver_name || !printer->attrs)
  {
    // Failed to allocate one of the required members...
    _papplPrinterDelete(printer);
    return (NULL);
  }

  if (papplSystemGetDefaultPrintGroup(system, print_group, sizeof(print_group)))
    papplPrinterSetPrintGroup(printer, print_group);

  // If the driver is "auto", figure out the proper driver name...
  if (!strcmp(driver_name, "auto") && system->autoadd_cb)
  {
    // If device_id is NULL, try to look it up...
    if (!printer->device_id && strncmp(device_uri, "file://", 7))
    {
      pappl_device_t	*device;	// Connection to printer

      papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Opening device for auto-setup.");
      if ((device = papplDeviceOpen(device_uri, "auto", papplLogDevice, system)) != NULL)
      {
        char	new_id[1024];		// New 1284 device ID

        if (papplDeviceGetID(device, new_id, sizeof(new_id)))
          printer->device_id = strdup(new_id);

        papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Closing device for auto-setup.");

        papplDeviceClose(device);
      }
    }

    if ((driver_name = (system->autoadd_cb)(printer_name, device_uri, printer->device_id, system->driver_cbdata)) == NULL)
    {
      errno = EIO;
      _papplPrinterDelete(printer);
      return (NULL);
    }
  }

  // Add static attributes...
    
  // charset-configured
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-configured", NULL, "utf-8");

  // charset-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_CHARSET), "charset-supported", sizeof(charset) / sizeof(charset[0]), NULL, charset);

  // client-info-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "client-info-supported", (int)(sizeof(client_info) / sizeof(client_info[0])), NULL, client_info);

  // compression-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "compression-supported", (int)(sizeof(compression) / sizeof(compression[0])), NULL, compression);

  // copies-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "copies-default", 1);

  // device-uuid
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uuid", NULL, uuid);

  // document-format-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "document-format-default", NULL, "application/octet-stream");

  // generated-natural-language-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "generated-natural-language-supported", NULL, "en");

  // ipp-versions-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "ipp-versions-supported", (cups_len_t)(sizeof(ipp_versions) / sizeof(ipp_versions[0])), NULL, ipp_versions);

  // job-hold-until-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-hold-until-default", NULL, "no-hold");

  // job-hold-until-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "job-hold-until-supported", (cups_len_t)(sizeof(job_hold_until) / sizeof(job_hold_until[0])), NULL, job_hold_until);

  // job-hold-until-time-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "job-hold-until-time-supported", 1);

  // job-ids-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "job-ids-supported", 1);

  // job-k-octets-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "job-k-octets-supported", 0, k_supported);

  // job-priority-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-default", 50);

  // job-priority-supported
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "job-priority-supported", 1);

  // job-sheets-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-default", NULL, "none");

  // job-sheets-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_NAME), "job-sheets-supported", NULL, "none");

  // multiple-document-handling-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-document-handling-supported", sizeof(multiple_document_handling) / sizeof(multiple_document_handling[0]), NULL, multiple_document_handling);

  // multiple-document-jobs-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "multiple-document-jobs-supported", 0);

  // multiple-operation-time-out
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "multiple-operation-time-out", 60);

  // multiple-operation-time-out-action
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "multiple-operation-time-out-action", NULL, "abort-job");

  // natural-language-configured
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "natural-language-configured", NULL, "en");

  // notify-events-default
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-events-default", NULL, "job-completed");

  // notify-events-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-events-supported", (int)(sizeof(_papplEvents) / sizeof(_papplEvents[0])), NULL, _papplEvents);

  // notify-lease-duration-default
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "notify-lease-duration-default", PAPPL_LEASE_DEFAULT);

  // notify-lease-duration-supported
  ippAddRange(printer->attrs, IPP_TAG_PRINTER, "notify-lease-duration-supported", 0, PAPPL_LEASE_MAX);

  // notify-max-events-supported
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "notify-max-events-supported", PAPPL_MAX_EVENTS);

  // notify-pull-method-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-pull-method-supported", NULL, "ippget");

  // operations-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "operations-supported", (int)(sizeof(operations) / sizeof(operations[0])), operations);

  // orientation-requested-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "orientation-requested-supported", (int)(sizeof(orientation_requested) / sizeof(orientation_requested[0])), orientation_requested);



  // pdl-override-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "pdl-override-supported", NULL, "attempted");

  // print-content-optimize-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-content-optimize-supported", (int)(sizeof(print_content_optimize) / sizeof(print_content_optimize[0])), NULL, print_content_optimize);

  // print-processing-attributes-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-processing-attributes-supported", (int)(sizeof(print_processing) / sizeof(print_processing[0])), NULL, print_processing);

  // print-quality-supported
  ippAddIntegers(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_ENUM, "print-quality-supported", (int)(sizeof(print_quality) / sizeof(print_quality[0])), print_quality);

  // print-scaling-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "print-scaling-supported", (int)(sizeof(print_scaling) / sizeof(print_scaling[0])), NULL, print_scaling);

  // printer-get-attributes-supported
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-get-attributes-supported", NULL, "document-format");

  // printer-id
  ippAddInteger(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-id", printer_id);

  // printer-info
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL, printer_name);

  // printer-name
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, printer_name);

  // printer-uuid
  ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uuid", NULL, uuid);

  // requesting-user-uri-supported
  ippAddBoolean(printer->attrs, IPP_TAG_PRINTER, "requesting-user-uri-supported", 1);

  // uri-security-supported
  if (system->options & PAPPL_SOPTIONS_NO_TLS)
    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", NULL, "none");
  else if (papplSystemGetTLSOnly(system))
    ippAddString(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", NULL, "tls");
  else
    ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "uri-security-supported", 2, NULL, uri_security);

  // which-jobs-supported
  ippAddStrings(printer->attrs, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_KEYWORD), "which-jobs-supported", sizeof(which_jobs) / sizeof(which_jobs[0]), NULL, which_jobs);





  // Initialize driver and driver-specific attributes...
  driver_attrs = NULL;
  _papplPrinterInitDriverData(&driver_data);

  pappl_pr_preset_data_t preset;
  // _papplPrinterInitPresetData(&preset);


  if (!(system->driver_cb)(system, driver_name, device_uri, device_id, &driver_data, &driver_attrs, system->driver_cbdata))
  {
    errno = EIO;
    _papplPrinterDelete(printer);
    return (NULL);
  }

  papplPrinterSetDriverData(printer, &driver_data, driver_attrs);

 

  ippDelete(driver_attrs);


  // Add the printer to the system...
  _papplSystemAddPrinter(system, printer, printer_id);


     // add the presets from their statefile...
  papplPresetAdd(system, printer);

        // fetch the size of presets over here ...
  cups_array_t *presets = printer->presets;
  const char * preset_sending[cupsArrayCount(presets)];


  /*
   *  add the presets in the printer->attrs object that I have created ...
   */


      _papplRWLockWrite(system);


  // for (cups_len_t x = 0; x < cupsArrayCount(presets); x++)
  // {
  //     pappl_pr_preset_data_t *preset_here = cupsArrayGetElement(presets, x);
  //     // Add attributes from your struct to the collection
  //     ipp_t* preset_collection = ippNew();
  //     ippAddString(presetCollection, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "name", NULL, presetData.name);

  //     ipp_attribute_t *collectionAttr = ippAddCollection(printer->attrs, IPP_TAG_PRINTER, "job-presets-supported", presetCollection);

  //     // Release the memory of the collection (it's duplicated in the attribute)
  //     ippDelete(presetCollection);
  // }

  /*
   * Code to add presets to collection and printer-> attrs...
  */

    ipp_attribute_t * final_preset = ippAddCollections(printer->attrs , IPP_TAG_PRINTER , "job-presets-supported", 
    cupsArrayCount(presets), NULL);
        


    for(int x=0; x < cupsArrayGetCount(presets); x++)
    {

        // fetch the preset object ...
        pappl_pr_preset_data_t * preset_here = cupsArrayGetElement(presets, x);
        ipp_t * col = ippNew();
        ippAddString(col, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "name",
              NULL, preset_here->name );

        ippSetCollection(printer->attrs, &final_preset, x, col);
        ippDelete(col);

    }

    // ippAddCollections(printer->attrs , IPP_TAG_PRINTER , "job-presets-supported", cupsArrayGetCount(presets) , all_presets);
    // add ipp one into the printer->attrs...
    // ippAddCollection(printer->attrs, IPP_TAG_PRINTER, "job-presets-supported", preset_collection);

    _papplRWUnlock(system);
    _papplSystemConfigChanged(system);



  // Do any post-creation work...
  if (system->create_cb)
    (system->create_cb)(printer, system->driver_cbdata);

  // Add socket listeners...
  if (system->options & PAPPL_SOPTIONS_RAW_SOCKET)
  {
    if (_papplPrinterAddRawListeners(printer) && system->is_running)
    {
      pthread_t	tid;			// Thread ID

      if (pthread_create(&tid, NULL, (void *(*)(void *))_papplPrinterRunRaw, printer))
      {
	// Unable to create client thread...
	papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "Unable to create raw listener thread: %s", strerror(errno));
      }
      else
      {
	// Detach the main thread from the raw thread to prevent hangs...
	pthread_detach(tid);

        _papplRWLockRead(printer);
	while (!printer->raw_active)
	{
	  _papplRWUnlock(printer);
	  usleep(1000);			// Wait for raw thread to start
	  _papplRWLockRead(printer);
	}
	_papplRWUnlock(printer);
      }
    }
  }




  // Add icons...
  _papplSystemAddPrinterIcons(system, printer);

  // Add web pages, if any...
  if (system->options & PAPPL_SOPTIONS_WEB_INTERFACE)
  {
    snprintf(path, sizeof(path), "%s/", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebHome, printer);

    snprintf(path, sizeof(path), "%s/cancelall", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebCancelAllJobs, printer);

    if (system->options & PAPPL_SOPTIONS_MULTI_QUEUE)
    {
      snprintf(path, sizeof(path), "%s/delete", printer->uriname);
      papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebDelete, printer);
    }

    snprintf(path, sizeof(path), "%s/config", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebConfig, printer);

    snprintf(path, sizeof(path), "%s/jobs", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebJobs, printer);

    snprintf(path, sizeof(path), "%s/media", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebMedia, printer);
    papplPrinterAddLink(printer, _PAPPL_LOC("Media"), path, PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);


// ---------------------------------------------//
    /*
    * add resource for creating and listing preset page ..
    * it must have a button with printing defaults...

    */
    
    snprintf(path, sizeof(path), "%s/presets", printer->uriname);
    // this add the content 
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterPreset, printer);
    // this add button link...
    papplPrinterAddLink(printer, _PAPPL_LOC("Presets"), path, PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);

    // Create button page ------ localhost:8000/printer/presets/create.
    snprintf(path, sizeof(path), "%s/presets/create", printer->uriname);
    // this add the content 
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterPresetCreate, printer);

    /*
     *  run a for loop and add all the preset links on each route ...
     */
    
    cups_len_t preset_iterator, preset_count;
    preset_count = cupsArrayGetCount(printer->presets);
    
    for(preset_iterator = 0; preset_iterator < preset_count; preset_iterator++)
    {

      //  printf("THE PATH VALUE THAT GETS GENERATED WITH EACH PRESET IS --- %s\n", path);

    
       pappl_pr_preset_data_t * preset = cupsArrayGetElement(printer->presets, preset_iterator);
       // run each preset on specific route ...

       resource_data_t *resource_data = calloc(1, sizeof(resource_data_t));
       resource_data->printer = printer;
       resource_data->preset_name = preset->name;

       // add the edit resource ...
       snprintf(path, sizeof(path), "%s/presets/%s/edit", printer->uriname , preset->name);
       papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterPresetEdit, resource_data);
       
       // add the copy resource ...
       snprintf(path, sizeof(path), "%s/presets/%s/copy", printer->uriname , preset->name);
       papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterPresetCopy, resource_data);

      //  // add the delete resource ...
      //  snprintf(path, sizeof(path), "%s/presets/%s/delete", printer->uriname , preset->name);
      //  papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterPresetEdit, resource_data);






    }
// ----------------------------------------------//



    snprintf(path, sizeof(path), "%s/printing", printer->uriname);
    papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebDefaults, printer);
    papplPrinterAddLink(printer, _PAPPL_LOC("Printing Defaults"), path, PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);

    if (printer->driver_data.has_supplies)
    {
      snprintf(path, sizeof(path), "%s/supplies", printer->uriname);
      papplSystemAddResourceCallback(system, path, "text/html", (pappl_resource_cb_t)_papplPrinterWebSupplies, printer);
      papplPrinterAddLink(printer, _PAPPL_LOC("Supplies"), path, PAPPL_LOPTIONS_STATUS);
    }
  }

  _papplSystemConfigChanged(system);

  // Return it!
  return (printer);
}




//
// '_papplPrinterDelete()' - Free memory associated with a printer.
//

void
_papplPrinterDelete(
    pappl_printer_t *printer)		// I - Printer
{
  int			i;		// Looping var
  _pappl_resource_t	*r;		// Current resource
  char			prefix[1024];	// Prefix for printer resources
  size_t		prefixlen;	// Length of prefix


  // Let USB/raw printing threads know to exit
  _papplRWLockWrite(printer);
  printer->is_deleted = true;

  while (printer->raw_active || printer->usb_active)
  {
    // Wait for threads to finish
    _papplRWUnlock(printer);
    usleep(1000);
    _papplRWLockRead(printer);
  }
  _papplRWUnlock(printer);

  // Close raw listener sockets...
  for (i = 0; i < printer->num_raw_listeners; i ++)
  {
#if _WIN32
    closesocket(printer->raw_listeners[i].fd);
#else
    close(printer->raw_listeners[i].fd);
#endif // _WIN32
    printer->raw_listeners[i].fd = -1;
  }

  printer->num_raw_listeners = 0;

  // Remove DNS-SD registrations...
  _papplPrinterUnregisterDNSSDNoLock(printer);

  // Remove printer-specific resources...
  snprintf(prefix, sizeof(prefix), "%s/", printer->uriname);
  prefixlen = strlen(prefix);

  // Note: System writer lock is already held when calling cupsArrayRemove
  // for the system's printer object, so we don't need a separate lock here
  // and can safely use cupsArrayGetFirst/Next...
  for (r = (_pappl_resource_t *)cupsArrayGetFirst(printer->system->resources); r; r = (_pappl_resource_t *)cupsArrayGetNext(printer->system->resources))
  {
    if (r->cbdata == printer || !strncmp(r->path, prefix, prefixlen))
      cupsArrayRemove(printer->system->resources, r);
  }

  // If applicable, call the delete function...
  if (printer->driver_data.delete_cb)
    (printer->driver_data.delete_cb)(printer, &printer->driver_data);

  // Delete jobs...
  cupsArrayDelete(printer->active_jobs);
  cupsArrayDelete(printer->completed_jobs);
  cupsArrayDelete(printer->all_jobs);

  // Free memory...
  free(printer->name);
  free(printer->dns_sd_name);
  free(printer->location);
  free(printer->geo_location);
  free(printer->organization);
  free(printer->org_unit);
  free(printer->resource);
  free(printer->device_id);
  free(printer->device_uri);
  free(printer->driver_name);
  free(printer->usb_storage);
  // here i am deleting the elements of preset array ....
  cupsArrayDelete(printer->presets);

  ippDelete(printer->driver_attrs);
  ippDelete(printer->attrs);

  cupsArrayDelete(printer->links);

  pthread_rwlock_destroy(&printer->rwlock);

  free(printer);
}


//
// 'papplPrinterDelete()' - Delete a printer.
//
// This function deletes a printer from a system, freeing all memory and
// canceling all jobs as needed.
//

void
papplPrinterDelete(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_system_t *system = printer->system;
					// System


  // Deliver delete event...
  papplSystemAddEvent(system, printer, NULL, PAPPL_EVENT_PRINTER_DELETED | PAPPL_EVENT_SYSTEM_CONFIG_CHANGED, NULL);

  // Remove the printer from the system object...
  _papplRWLockWrite(system);
  cupsArrayRemove(system->printers, printer);
  _papplRWUnlock(system);

  _papplSystemConfigChanged(system);
}


//
// 'papplPrinterOpenFile()' - Create or open a file for a printer.
//
// This function creates, opens, or removes a file for a printer.  The "fname"
// and "fnamesize" arguments specify the location and size of a buffer to store
// the printer filename, which incorporates the "directory", printer ID,
// resource name, and "ext" values.  The resource name is "sanitized" to only
// contain alphanumeric characters.
//
// The "mode" argument is "r" to read an existing printer file, "w" to write a
// new printer file, or "x" to remove an exitsing printer file.  New files are
// created with restricted permissions for security purposes.
//
// For the "r" and "w" modes, the return value is the file descriptor number on
// success or `-1` on error.  For the "x" mode, the return value is `0` on
// success and `-1` on error.  The `errno` variable is set appropriately on
// error.
//

// int					// O - File descriptor or -1 on error
// papplPrinterOpenFile(
//     pappl_printer_t *printer,		// I - Printer
//     char            *fname,		// I - Filename buffer
//     size_t          fnamesize,		// I - Size of filename buffer
//     const char      *directory,		// I - Directory to store in (`NULL` for default)
//     const char      *resname,		// I - Resource name
//     const char      *ext,		// I - Extension (`NULL` for none)
//     const char      *mode)		// I - Open mode - "r" for reading or "w" for writing
// {
//   char	name[64],			// "Safe" filename
// 	*nameptr;			// Pointer into filename

//   // printf("papplPrinterOpenFile is called \n");
//   // printf("    \n");
//   // printf("papplPrinterOpenFile , the directory inside printer->system->directory -- -- %s\n",printer->system->directory);


//   // Range check input...
//   if (!printer || !fname || fnamesize < 256 || !resname || !mode)
//   {
//     if (fname)
//       *fname = '\0';

//     return (-1);
//   }

//   // Make sure the spool directory exists...
//   if (!directory)
//     directory = printer->system->directory;

//   if (access(directory, X_OK))
//   {
//     if (errno == ENOENT)
//     {
//       // Spool directory does not exist, might have been deleted...
//       if (mkdir(directory, 0777))
//       {
//         papplLogPrinter(printer, PAPPL_LOGLEVEL_FATAL, "Unable to create spool directory '%s': %s", directory, strerror(errno));
//         return (-1);
//       }
//     }
//     else
//     {
//       papplLogPrinter(printer, PAPPL_LOGLEVEL_FATAL, "Unable to access spool directory '%s': %s", directory, strerror(errno));
//       return (-1);
//     }
//   }

//   // Make a name from the resource name argument...
//   for (nameptr = name; *resname && nameptr < (name + sizeof(name) - 1); resname ++)
//   {
//       // printf(" we are not creating the file ,...\n");
//     if (isalnum(*resname & 255) || *resname == '-' || *resname == '.')
//     {
//       *nameptr++ = (char)tolower(*resname & 255);
//     }
//     else
//     {
//       *nameptr++ = '_';

//       while (resname[1] && !isalnum(resname[1] & 255) && resname[1] != '-' && resname[1] != '.')
//         resname ++;
//     }
//   }
//   // printf("    \n");
//   // printf("papplPrinterOpenFile created the file whose name is  -- %s\n", resname);

//   *nameptr = '\0';


//   // Create a filename...
//   if (ext)
//   {

//     snprintf(fname, fnamesize, "%s/p%05d-%s.%s", directory, printer->printer_id, name, ext);

//     printf(" the file from which we are reading is   \n");
//     printf(fname, fnamesize, "---- %s/p%05d-%s.%s\n", directory, printer->printer_id, name, ext);
//   }
    
//   else
//   {
//     snprintf(fname, fnamesize, "%s/p%05d-%s", directory, printer->printer_id, name);
// printf("  else block get executed ...  \n");
//     printf(fname, fnamesize, "----%s/p%05d-%s\n", directory, printer->printer_id, name);
//   }


//   if (!strcmp(mode, "r"))
//   {
//     printf("the 'r' checking is working properly.... \n ");
//     printf("the value of fname variable is ---- %s\n", fname);
//     int something = (open(fname, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_BINARY));
//     printf("the value of something variable introduced is -- %d\n", something);
//     return something;

//   }
//   else if (!strcmp(mode, "w"))
//     return (open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC | O_BINARY, 0600));
//   else if (!strcmp(mode, "x"))
//     return (unlink(fname));
//   else
//     return (-1);
// }


int					// O - File descriptor or -1 on error
papplPrinterOpenFile(
    pappl_printer_t *printer,		// I - Printer
    char            *fname,		// I - Filename buffer
    size_t          fnamesize,		// I - Size of filename buffer
    const char      *directory,		// I - Directory to store in (`NULL` for default)
    const char      *resname,		// I - Resource name
    const char      *ext,		// I - Extension (`NULL` for none)
    const char      *mode)		// I - Open mode - "r" for reading or "w" for writing
{
  char	name[64],			// "Safe" filename
	*nameptr;			// Pointer into filename


  // Range check input...
  if (!printer || !fname || fnamesize < 256 || !resname || !mode)
  {
    if (fname)
      *fname = '\0';

    return (-1);
  }

  // Make sure the spool directory exists...
  if (!directory)
    directory = printer->system->directory;

  if (mkdir(directory, 0777) && errno != EEXIST)
  {
    papplLogPrinter(printer, PAPPL_LOGLEVEL_FATAL, "Unable to create spool directory '%s': %s", directory, strerror(errno));
    return (-1);
  }

  // Make a name from the resource name argument...
  for (nameptr = name; *resname && nameptr < (name + sizeof(name) - 1); resname ++)
  {
    if (isalnum(*resname & 255) || *resname == '-' || *resname == '.')
    {
      *nameptr++ = (char)tolower(*resname & 255);
    }
    else
    {
      *nameptr++ = '_';

      while (resname[1] && !isalnum(resname[1] & 255) && resname[1] != '-' && resname[1] != '.')
        resname ++;
    }
  }

  *nameptr = '\0';

  // Create a filename...
  if (ext)
  {
    printf(" \n");

    printf("PRINTING THE VALUES INSIDE FNAME FILECREATION DIRECTORY\n");
    printf("The value of directory -- %s\n", directory);
    printf("The value of Printer id is -- %d \n", printer->printer_id);
    printf("The value of name variable is -- %s \n", name);
    printf(" \n");



    snprintf(fname, fnamesize, "%s/p%05d-%s.%s", directory, printer->printer_id, name, ext);

  }
  else
    snprintf(fname, fnamesize, "%s/p%05d-%s", directory, printer->printer_id, name);

  if (!strcmp(mode, "r"))
  {
    printf("The VALUE OF FNAME VARIABLE IS --- %s\n",fname);
        return (open(fname, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_BINARY));
  }

  else if (!strcmp(mode, "w"))
    return (open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC | O_BINARY, 0600));
  else if (!strcmp(mode, "x"))
    return (unlink(fname));
  else
    return (-1);
}

// printer open file pappl for presets...
int					// O - File descriptor or -1 on error
papplPrinterOpenFilePreset(
    pappl_printer_t *printer,		// I - Printer
    char            *fname,		// I - Filename buffer
    size_t          fnamesize,		// I - Size of filename buffer
    const char      *directory,		// I - Directory to store in (`NULL` for default)
    const char      *resname,		// I - Resource name
    const char      *ext,		// I - Extension (`NULL` for none)
    const char      *mode)		// I - Open mode - "r" for reading or "w" for writing
{
  char	name[64],			// "Safe" filename
	*nameptr;			// Pointer into filename

  // printf("papplPrinterOpenFile is called \n");
  printf("    \n");
  printf("papplPrinterOpenFile , the directory inside printer->system->directory -- -- %s\n",printer->system->directory);


  // Range check input...
  if (!printer || !fname || fnamesize < 256 || !resname || !mode)
  {
    if (fname)
      *fname = '\0';

    return (-1);
  }

  // Make sure the spool directory exists...
  if (!directory)
    directory = printer->system->directory;

  if (access(directory, X_OK))
  {
    if (errno == ENOENT)
    {
      // Spool directory does not exist, might have been deleted...
      if (mkdir(directory, 0777))
      {
        papplLogPrinter(printer, PAPPL_LOGLEVEL_FATAL, "Unable to create spool directory '%s': %s", directory, strerror(errno));
        return (-1);
      }
    }
    else
    {
      papplLogPrinter(printer, PAPPL_LOGLEVEL_FATAL, "Unable to access spool directory '%s': %s", directory, strerror(errno));
      return (-1);
    }
  }

  // Make a name from the resource name argument...
  for (nameptr = name; *resname && nameptr < (name + sizeof(name) - 1); resname ++)
  {
      // printf(" we are not creating the file ,...\n");
    if (isalnum(*resname & 255) || *resname == '-' || *resname == '.')
    {
      *nameptr++ = (char)tolower(*resname & 255);
    }
    else
    {
      *nameptr++ = '_';

      while (resname[1] && !isalnum(resname[1] & 255) && resname[1] != '-' && resname[1] != '.')
        resname ++;
    }
  }
  printf("    \n");
  printf("papplPrinterOpenFile created the file whose name is  -- %s\n", resname);

  *nameptr = '\0';


  // Create a filename...
  if (ext)
  {

    snprintf(fname, fnamesize, "%s/%s.%s", directory,  name, ext);

    printf(" if block get executed ...   \n");
    printf(fname, fnamesize, "---- %s/p%05d-%s.%s\n", directory, printer->printer_id, name, ext);
  }
    
  else
  {
    snprintf(fname, fnamesize, "%s/p%05d-%s", directory, printer->printer_id, name);
printf("  else block get executed ...  \n");
    printf(fname, fnamesize, "----%s/p%05d-%s\n", directory, printer->printer_id, name);
  }


  if (!strcmp(mode, "r"))
  {
    printf("the 'r' checking is working properly.... \n ");
    printf("the value of fname variable is ---- %s\n", fname);
    int something = (open(fname, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_BINARY));
    printf("the value of something variable introduced is -- %d\n", something);
    return something;

  }
  else if (!strcmp(mode, "w"))
    return (open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC | O_BINARY, 0600));
  else if (!strcmp(mode, "x"))
    return (unlink(fname));
  else
    return (-1);
}




//
// 'compare_active_jobs()' - Compare two active jobs.
//

static int				// O - Result of comparison
compare_active_jobs(pappl_job_t *a,	// I - First job
                    pappl_job_t *b)	// I - Second job
{
  return (b->job_id - a->job_id);
}


//
// 'compare_jobs()' - Compare two jobs.
//

static int				// O - Result of comparison
compare_all_jobs(pappl_job_t *a,	// I - First job
                 pappl_job_t *b)	// I - Second job
{
  return (b->job_id - a->job_id);
}


//
// 'compare_completed_jobs()' - Compare two completed jobs.
//

static int				// O - Result of comparison
compare_completed_jobs(pappl_job_t *a,	// I - First job
                       pappl_job_t *b)	// I - Second job
{
  return (b->job_id - a->job_id);
}
