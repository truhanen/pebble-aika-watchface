#pragma once
#include <pebble.h>
#include "time.h"


/*
 * For the specified GDrawCommandImage, recolors it with
 * the specified fill and stroke colors
 */
extern void gdraw_command_image_recolor(GDrawCommandImage *img, GColor fill_color, GColor stroke_color);

/*
 * Loads a PDC resource and scales it so that its ink (points plus stroke) is exactly
 * WL_ICON_H high, keeping the aspect: the view box of the files carries padding, so
 * each icon is normalised by its own drawn extent. NULL if the resource is missing.
 */
extern GDrawCommandImage *icon_image_create_with_resource(uint32_t resource_id);

/*
 * Returns the current time in Swatch Internet Time "beats"
 */
extern int time_get_beats(const struct tm *tm);

  /*
   * Checks if any of the specified health activites exist in the specified time range
   */
  extern bool is_health_metric_accessible(HealthMetric metric);

  /*
   * Returns true if the user is sleeping, false otherwise
   */
  extern bool is_user_sleeping();
