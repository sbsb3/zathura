/* SPDX-License-Identifier: Zlib */

#include "content-bbox.h"

#define EPS 1e-9

static void test_percentile_single_value(void) {
  const double values[] = {5.0};
  /* n=1 must return the single value regardless of the requested percentile */
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 1, 0.0), 5.0, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 1, 50.0), 5.0, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 1, 90.0), 5.0, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 1, 100.0), 5.0, EPS);
}

static void test_percentile_two_values(void) {
  const double values[] = {0.0, 10.0};
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 2, 0.0), 0.0, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 2, 25.0), 2.5, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 2, 50.0), 5.0, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 2, 100.0), 10.0, EPS);
}

static void test_percentile_interpolation(void) {
  /* passed unsorted on purpose, to exercise the internal sort */
  const double values[] = {8.0, 2.0, 6.0, 4.0};
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 4, 0.0), 2.0, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 4, 25.0), 3.5, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 4, 50.0), 5.0, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 4, 100.0), 8.0, EPS);
}

static void test_percentile_ten_values(void) {
  /* n=10, exact rank hits: rank = p/100 * 9 */
  const double values[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 10, 0.0), 10.0, EPS);
  /* p=90 -> rank=8.1 -> between values[8]=90 and values[9]=100, frac=0.1 */
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 10, 90.0), 91.0, EPS);
  g_assert_cmpfloat_with_epsilon(percentile_linear(values, 10, 100.0), 100.0, EPS);
}

static void test_aggregate_single_rect(void) {
  /* aggregating a single sample must return it unchanged, regardless of the
   * percentile setting, since percentile_linear(n=1) always returns v[0] */
  const zathura_rectangle_t rects[] = {{.x1 = 12, .y1 = 34, .x2 = 56, .y2 = 78}};

  const zathura_rectangle_t agg90 = content_bbox_aggregate(rects, 1, 90.0);
  g_assert_cmpfloat_with_epsilon(agg90.x1, 12.0, EPS);
  g_assert_cmpfloat_with_epsilon(agg90.y1, 34.0, EPS);
  g_assert_cmpfloat_with_epsilon(agg90.x2, 56.0, EPS);
  g_assert_cmpfloat_with_epsilon(agg90.y2, 78.0, EPS);

  const zathura_rectangle_t agg50 = content_bbox_aggregate(rects, 1, 50.0);
  g_assert_cmpfloat_with_epsilon(agg50.x1, 12.0, EPS);
  g_assert_cmpfloat_with_epsilon(agg50.y1, 34.0, EPS);
  g_assert_cmpfloat_with_epsilon(agg50.x2, 56.0, EPS);
  g_assert_cmpfloat_with_epsilon(agg50.y2, 78.0, EPS);
}

static void test_aggregate_trims_outlier(void) {
  /* Two "typical" pages and one wide/tall outlier page (e.g. a landscape
   * figure). At the 90th percentile, the outlier must not dominate either the
   * origin or the extent -- x and y are aggregated independently, so this also
   * confirms the aggregation is a full rectangle (needed for
   * recalc_rectangle()'s rotation handling), not just a width. */
  const zathura_rectangle_t rects[] = {
      {.x1 = 10, .y1 = 10, .x2 = 100, .y2 = 200},
      {.x1 = 12, .y1 = 12, .x2 = 105, .y2 = 205},
      {.x1 = 50, .y1 = 50, .x2 = 300, .y2 = 400}, /* outlier: wide and tall */
  };

  const zathura_rectangle_t agg = content_bbox_aggregate(rects, 3, 90.0);

  /* origin (x1/y1) uses the (100-90)=10th percentile: rank=0.2 of
   * [10,12,50] -> 10 + 0.2*(12-10) = 10.4 */
  g_assert_cmpfloat_with_epsilon(agg.x1, 10.4, EPS);
  g_assert_cmpfloat_with_epsilon(agg.y1, 10.4, EPS);

  /* extents use the 90th percentile of the *widths* [90,93,250]: rank=1.8
   * -> 93 + 0.8*(250-93) = 218.6, and of the heights [190,193,350]
   * -> 193 + 0.8*(350-193) = 318.6; the far edges are origin + extent */
  g_assert_cmpfloat_with_epsilon(agg.x2, 10.4 + 218.6, EPS);
  g_assert_cmpfloat_with_epsilon(agg.y2, 10.4 + 318.6, EPS);

  /* the outlier's extremes must be trimmed away, not dominate the aggregate */
  g_assert_cmpfloat(agg.x2, <, 300.0);
  g_assert_cmpfloat(agg.y2, <, 400.0);
  g_assert_cmpfloat(agg.x1, <, 50.0);
  g_assert_cmpfloat(agg.y1, <, 50.0);
}

static void test_aggregate_mirrored_margins(void) {
  /* A book with mirrored inner/outer margins: identical 415pt text columns
   * that start 18pt further right on every other page. The column must come
   * out 415pt wide (each page is drawn shifted onto it), not 433pt -- folding
   * the mirror offset into the column is exactly the bug that left an
   * alternating empty strip down one side of the viewport. */
  const zathura_rectangle_t rects[] = {
      {.x1 = 36, .y1 = 50, .x2 = 451, .y2 = 700}, {.x1 = 54, .y1 = 50, .x2 = 469, .y2 = 700},
      {.x1 = 36, .y1 = 50, .x2 = 451, .y2 = 700}, {.x1 = 54, .y1 = 50, .x2 = 469, .y2 = 700},
  };

  const zathura_rectangle_t agg = content_bbox_aggregate(rects, 4, 90.0);

  g_assert_cmpfloat_with_epsilon(agg.x1, 36.0, EPS);
  g_assert_cmpfloat_with_epsilon(agg.x2 - agg.x1, 415.0, EPS);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/content-bbox/percentile_single_value", test_percentile_single_value);
  g_test_add_func("/content-bbox/percentile_two_values", test_percentile_two_values);
  g_test_add_func("/content-bbox/percentile_interpolation", test_percentile_interpolation);
  g_test_add_func("/content-bbox/percentile_ten_values", test_percentile_ten_values);
  g_test_add_func("/content-bbox/aggregate_single_rect", test_aggregate_single_rect);
  g_test_add_func("/content-bbox/aggregate_trims_outlier", test_aggregate_trims_outlier);
  g_test_add_func("/content-bbox/aggregate_mirrored_margins", test_aggregate_mirrored_margins);
  return g_test_run();
}
