/*  ADMesh -- process triangulated solid meshes
 *  Copyright (C) 1995, 1996  Anthony D. Martin <amartin@engr.csulb.edu>
 *  Copyright (C) 2013, 2014  several contributors, see AUTHORS
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Questions, comments, suggestions, etc to
 *           https://github.com/admesh/admesh/issues
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "stl.h"

static void stl_rotate(float *x, float *y, const double c, const double s);
static float get_area(stl_facet *facet);
static float get_volume(stl_file *stl);


void
stl_verify_neighbors(stl_file *stl) {
  int i;
  int j;
  stl_edge edge_a;
  stl_edge edge_b;
  int neighbor;
  int vnot;

  if (stl->error) return;

  stl->stats.backwards_edges = 0;

  for(i = 0; i < stl->stats.number_of_facets; i++) {
    for(j = 0; j < 3; j++) {
      edge_a.p1 = stl->facet_start[i].vertex[j];
      edge_a.p2 = stl->facet_start[i].vertex[(j + 1) % 3];
      neighbor = stl->neighbors_start[i].neighbor[j];
      vnot = stl->neighbors_start[i].which_vertex_not[j];

      if(neighbor == -1)
        continue;		/* this edge has no neighbor... Continue. */
      if(vnot < 3) {
        edge_b.p1 = stl->facet_start[neighbor].vertex[(vnot + 2) % 3];
        edge_b.p2 = stl->facet_start[neighbor].vertex[(vnot + 1) % 3];
      } else {
        stl->stats.backwards_edges += 1;
        edge_b.p1 = stl->facet_start[neighbor].vertex[(vnot + 1) % 3];
        edge_b.p2 = stl->facet_start[neighbor].vertex[(vnot + 2) % 3];
      }
      if (edge_a.p1 != edge_b.p1 || edge_a.p2 != edge_b.p2) {
        /* These edges should match but they don't.  Print results. */
        printf("edge %d of facet %d doesn't match edge %d of facet %d\n",
               j, i, vnot + 1, neighbor);
        stl_write_facet(stl, (char*)"first facet", i);
        stl_write_facet(stl, (char*)"second facet", neighbor);
      }
    }
  }
}

void stl_translate(stl_file *stl, float x, float y, float z)
{
  if (stl->error)
  	return;

  stl_vertex new_min(x, y, z);
  stl_vertex shift = new_min - stl->stats.min;
  for (int i = 0; i < stl->stats.number_of_facets; ++ i)
    for (int j = 0; j < 3; ++ j)
      stl->facet_start[i].vertex[j] += shift;
  stl->stats.min = new_min;
  stl->stats.max += shift;
  stl_invalidate_shared_vertices(stl);
}

/* Translates the stl by x,y,z, relatively from wherever it is currently */
void stl_translate_relative(stl_file *stl, float x, float y, float z)
{
  if (stl->error)
  	return;

  stl_vertex shift(x, y, z);
  for (int i = 0; i < stl->stats.number_of_facets; ++ i)
    for (int j = 0; j < 3; ++ j)
      stl->facet_start[i].vertex[j] += shift;
  stl->stats.min += shift;
  stl->stats.max += shift;
  stl_invalidate_shared_vertices(stl);
}

void stl_scale_versor(stl_file *stl, const stl_vertex &versor)
{
  if (stl->error)
  	return;

  // Scale extents.
  auto s = versor.array();
  stl->stats.min.array() *= s;
  stl->stats.max.array() *= s;
  // Scale size.
  stl->stats.size.array() *= s;
  // Scale volume.
  if (stl->stats.volume > 0.0)
    stl->stats.volume *= versor(0) * versor(1) * versor(2);
  // Scale the mesh.
  for (int i = 0; i < stl->stats.number_of_facets; ++ i)
    for (int j = 0; j < 3; ++ j)
      stl->facet_start[i].vertex[j].array() *= s;
  stl_invalidate_shared_vertices(stl);
}

static void calculate_normals(stl_file *stl) 
{
  if (stl->error)
  	return;

  stl_normal normal;
  for(uint32_t i = 0; i < stl->stats.number_of_facets; i++) {
    stl_calculate_normal(normal, &stl->facet_start[i]);
    stl_normalize_vector(normal);
    stl->facet_start[i].normal = normal;
  }
}

void stl_transform(stl_file *stl, float *trafo3x4) {
  int i_face, i_vertex;
  if (stl->error)
    return;
  for (i_face = 0; i_face < stl->stats.number_of_facets; ++ i_face) {
    stl_vertex *vertices = stl->facet_start[i_face].vertex;
    for (i_vertex = 0; i_vertex < 3; ++ i_vertex) {
      stl_vertex &v_dst = vertices[i_vertex];
      stl_vertex  v_src = v_dst;
      v_dst(0) = trafo3x4[0] * v_src(0) + trafo3x4[1] * v_src(1) + trafo3x4[2]  * v_src(2) + trafo3x4[3];
      v_dst(1) = trafo3x4[4] * v_src(0) + trafo3x4[5] * v_src(1) + trafo3x4[6]  * v_src(2) + trafo3x4[7];
      v_dst(2) = trafo3x4[8] * v_src(0) + trafo3x4[9] * v_src(1) + trafo3x4[10] * v_src(2) + trafo3x4[11];
    }
  }
  stl_get_size(stl);
  calculate_normals(stl);
}

void stl_transform(stl_file *stl, const Eigen::Transform<double, 3, Eigen::Affine, Eigen::DontAlign>& t)
{
    if (stl->error)
        return;

    unsigned int vertices_count = 3 * (unsigned int)stl->stats.number_of_facets;
    if (vertices_count == 0)
        return;

    Eigen::MatrixXf src_vertices(3, vertices_count);
    stl_facet* facet_ptr = stl->facet_start;
    unsigned int v_id = 0;
    while (facet_ptr < stl->facet_start + stl->stats.number_of_facets)
    {
        for (int i = 0; i < 3; ++i)
        {
            ::memcpy((void*)src_vertices.col(v_id).data(), (const void*)&facet_ptr->vertex[i], 3 * sizeof(float));
            ++v_id;
        }
        facet_ptr += 1;
    }

    Eigen::MatrixXf dst_vertices(3, vertices_count);
    dst_vertices = t.cast<float>() * src_vertices.colwise().homogeneous();

    facet_ptr = stl->facet_start;
    v_id = 0;
    while (facet_ptr < stl->facet_start + stl->stats.number_of_facets)
    {
        for (int i = 0; i < 3; ++i)
        {
            ::memcpy((void*)&facet_ptr->vertex[i], (const void*)dst_vertices.col(v_id).data(), 3 * sizeof(float));
            ++v_id;
        }
        facet_ptr += 1;
    }

    stl_get_size(stl);
    calculate_normals(stl);
}

void
stl_rotate_x(stl_file *stl, float angle) {
  int i;
  int j;
  double radian_angle = (angle / 180.0) * M_PI;
  double c = cos(radian_angle);
  double s = sin(radian_angle);

  if (stl->error) return;

  for(i = 0; i < stl->stats.number_of_facets; i++) {
    for(j = 0; j < 3; j++) {
      stl_rotate(&stl->facet_start[i].vertex[j](1),
                 &stl->facet_start[i].vertex[j](2), c, s);
    }
  }
  stl_get_size(stl);
  calculate_normals(stl);
}

void
stl_rotate_y(stl_file *stl, float angle) {
  int i;
  int j;
  double radian_angle = (angle / 180.0) * M_PI;
  double c = cos(radian_angle);
  double s = sin(radian_angle);

  if (stl->error) return;

  for(i = 0; i < stl->stats.number_of_facets; i++) {
    for(j = 0; j < 3; j++) {
      stl_rotate(&stl->facet_start[i].vertex[j](2),
                 &stl->facet_start[i].vertex[j](0), c, s);
    }
  }
  stl_get_size(stl);
  calculate_normals(stl);
}

void
stl_rotate_z(stl_file *stl, float angle) {
  int i;
  int j;
  double radian_angle = (angle / 180.0) * M_PI;
  double c = cos(radian_angle);
  double s = sin(radian_angle);

  if (stl->error) return;

  for(i = 0; i < stl->stats.number_of_facets; i++) {
    for(j = 0; j < 3; j++) {
      stl_rotate(&stl->facet_start[i].vertex[j](0),
                 &stl->facet_start[i].vertex[j](1), c, s);
    }
  }
  stl_get_size(stl);
  calculate_normals(stl);
}



static void
stl_rotate(float *x, float *y, const double c, const double s) {
  double xold = *x;
  double yold = *y;
  *x = float(c * xold - s * yold);
  *y = float(s * xold + c * yold);
}

void stl_get_size(stl_file *stl)
{
  if (stl->error || stl->stats.number_of_facets == 0)
  	return;
  stl->stats.min = stl->facet_start[0].vertex[0];
  stl->stats.max = stl->stats.min;
  for (int i = 0; i < stl->stats.number_of_facets; ++ i) {
  	const stl_facet &face = stl->facet_start[i];
    for (int j = 0; j < 3; ++ j) {
      stl->stats.min = stl->stats.min.cwiseMin(face.vertex[j]);
      stl->stats.max = stl->stats.max.cwiseMax(face.vertex[j]);
    }
  }
  stl->stats.size = stl->stats.max - stl->stats.min;
  stl->stats.bounding_diameter = stl->stats.size.norm();
}

void stl_mirror_xy(stl_file *stl)
{
  if (stl->error) 
  	return;

  for(int i = 0; i < stl->stats.number_of_facets; i++) {
    for(int j = 0; j < 3; j++) {
      stl->facet_start[i].vertex[j](2) *= -1.0;
    }
  }
  float temp_size = stl->stats.min(2);
  stl->stats.min(2) = stl->stats.max(2);
  stl->stats.max(2) = temp_size;
  stl->stats.min(2) *= -1.0;
  stl->stats.max(2) *= -1.0;
  stl_reverse_all_facets(stl);
  stl->stats.facets_reversed -= stl->stats.number_of_facets;  /* for not altering stats */
}

void stl_mirror_yz(stl_file *stl)
{
  if (stl->error) return;

  for (int i = 0; i < stl->stats.number_of_facets; i++) {
    for (int j = 0; j < 3; j++) {
      stl->facet_start[i].vertex[j](0) *= -1.0;
    }
  }
  float temp_size = stl->stats.min(0);
  stl->stats.min(0) = stl->stats.max(0);
  stl->stats.max(0) = temp_size;
  stl->stats.min(0) *= -1.0;
  stl->stats.max(0) *= -1.0;
  stl_reverse_all_facets(stl);
  stl->stats.facets_reversed -= stl->stats.number_of_facets;  /* for not altering stats */
}

void stl_mirror_xz(stl_file *stl)
{
  if (stl->error)
  	return;

  for (int i = 0; i < stl->stats.number_of_facets; i++) {
    for (int j = 0; j < 3; j++) {
      stl->facet_start[i].vertex[j](1) *= -1.0;
    }
  }
  float temp_size = stl->stats.min(1);
  stl->stats.min(1) = stl->stats.max(1);
  stl->stats.max(1) = temp_size;
  stl->stats.min(1) *= -1.0;
  stl->stats.max(1) *= -1.0;
  stl_reverse_all_facets(stl);
  stl->stats.facets_reversed -= stl->stats.number_of_facets;  /* for not altering stats */
}

static float get_volume(stl_file *stl)
{
  if (stl->error)
  	return 0;

  // Choose a point, any point as the reference.
  stl_vertex p0 = stl->facet_start[0].vertex[0];
  float volume = 0.f;
  for(uint32_t i = 0; i < stl->stats.number_of_facets; ++ i) {
    // Do dot product to get distance from point to plane.
    float height = stl->facet_start[i].normal.dot(stl->facet_start[i].vertex[0] - p0);
    float area   = get_area(&stl->facet_start[i]);
    volume += (area * height) / 3.0f;
  }
  return volume;
}

void stl_calculate_volume(stl_file *stl)
{
  if (stl->error) return;
  stl->stats.volume = get_volume(stl);
  if(stl->stats.volume < 0.0) {
    stl_reverse_all_facets(stl);
    stl->stats.volume = -stl->stats.volume;
  }
}

static float get_area(stl_facet *facet)
{
  /* cast to double before calculating cross product because large coordinates
     can result in overflowing product
    (bad area is responsible for bad volume and bad facets reversal) */
  double cross[3][3];
  for (int i = 0; i < 3; i++) {
    cross[i][0]=(((double)facet->vertex[i](1) * (double)facet->vertex[(i + 1) % 3](2)) -
                 ((double)facet->vertex[i](2) * (double)facet->vertex[(i + 1) % 3](1)));
    cross[i][1]=(((double)facet->vertex[i](2) * (double)facet->vertex[(i + 1) % 3](0)) -
                 ((double)facet->vertex[i](0) * (double)facet->vertex[(i + 1) % 3](2)));
    cross[i][2]=(((double)facet->vertex[i](0) * (double)facet->vertex[(i + 1) % 3](1)) -
                 ((double)facet->vertex[i](1) * (double)facet->vertex[(i + 1) % 3](0)));
  }

  stl_normal sum;
  sum(0) = cross[0][0] + cross[1][0] + cross[2][0];
  sum(1) = cross[0][1] + cross[1][1] + cross[2][1];
  sum(2) = cross[0][2] + cross[1][2] + cross[2][2];

  // This should already be done.  But just in case, let's do it again.
  //FIXME this is questionable. the "sum" normal should be accurate, while the normal "n" may be calculated with a low accuracy.
  stl_normal n;
  stl_calculate_normal(n, facet);
  stl_normalize_vector(n);
  return 0.5f * n.dot(sum);
}

void stl_repair(stl_file *stl,
                int fixall_flag,
                int exact_flag,
                int tolerance_flag,
                float tolerance,
                int increment_flag,
                float increment,
                int nearby_flag,
                int iterations,
                int remove_unconnected_flag,
                int fill_holes_flag,
                int normal_directions_flag,
                int normal_values_flag,
                int reverse_all_flag,
                int verbose_flag) {
  
  int i;
  int last_edges_fixed = 0;

  if (stl->error) return;

  if(exact_flag || fixall_flag || nearby_flag || remove_unconnected_flag
      || fill_holes_flag || normal_directions_flag) {
    if (verbose_flag)
      printf("Checking exact...\n");
    exact_flag = 1;
    stl_check_facets_exact(stl);
    stl->stats.facets_w_1_bad_edge =
      (stl->stats.connected_facets_2_edge -
       stl->stats.connected_facets_3_edge);
    stl->stats.facets_w_2_bad_edge =
      (stl->stats.connected_facets_1_edge -
       stl->stats.connected_facets_2_edge);
    stl->stats.facets_w_3_bad_edge =
      (stl->stats.number_of_facets -
       stl->stats.connected_facets_1_edge);
  }

  if(nearby_flag || fixall_flag) {
    if(!tolerance_flag) {
      tolerance = stl->stats.shortest_edge;
    }
    if(!increment_flag) {
      increment = stl->stats.bounding_diameter / 10000.0;
    }

    if(stl->stats.connected_facets_3_edge < stl->stats.number_of_facets) {
      for(i = 0; i < iterations; i++) {
        if(stl->stats.connected_facets_3_edge <
            stl->stats.number_of_facets) {
          if (verbose_flag)
            printf("\
Checking nearby. Tolerance= %f Iteration=%d of %d...",
                 tolerance, i + 1, iterations);
          stl_check_facets_nearby(stl, tolerance);
          if (verbose_flag)
            printf("  Fixed %d edges.\n",
                 stl->stats.edges_fixed - last_edges_fixed);
          last_edges_fixed = stl->stats.edges_fixed;
          tolerance += increment;
        } else {
          if (verbose_flag)
            printf("\
All facets connected.  No further nearby check necessary.\n");
          break;
        }
      }
    } else {
      if (verbose_flag)
        printf("All facets connected.  No nearby check necessary.\n");
    }
  }

  if(remove_unconnected_flag || fixall_flag || fill_holes_flag) {
    if(stl->stats.connected_facets_3_edge <  stl->stats.number_of_facets) {
      if (verbose_flag)
        printf("Removing unconnected facets...\n");
      stl_remove_unconnected_facets(stl);
    } else
      if (verbose_flag)
        printf("No unconnected need to be removed.\n");
  }

  if(fill_holes_flag || fixall_flag) {
    if(stl->stats.connected_facets_3_edge <  stl->stats.number_of_facets) {
      if (verbose_flag)
        printf("Filling holes...\n");
      stl_fill_holes(stl);
    } else
      if (verbose_flag)
        printf("No holes need to be filled.\n");
  }

  if(reverse_all_flag) {
    if (verbose_flag)
      printf("Reversing all facets...\n");
    stl_reverse_all_facets(stl);
  }

  if(normal_directions_flag || fixall_flag) {
    if (verbose_flag)
      printf("Checking normal directions...\n");
    stl_fix_normal_directions(stl);
  }

  if(normal_values_flag || fixall_flag) {
    if (verbose_flag)
      printf("Checking normal values...\n");
    stl_fix_normal_values(stl);
  }

  /* Always calculate the volume.  It shouldn't take too long */
  if (verbose_flag)
    printf("Calculating volume...\n");
  stl_calculate_volume(stl);

  if(exact_flag) {
    if (verbose_flag)
      printf("Verifying neighbors...\n");
    stl_verify_neighbors(stl);
  }
}
