// fgt — fast Gauss transforms
// Copyright (C) 2016 Peter J. Gadomski <pete.gadomski@gmail.com>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

#include <cmath>
#include <limits>

#include "cluster.hpp"

namespace fgt {

Clustering cluster(const MatrixRef points, size_t nclusters, double epsilon,
                   const MatrixRef starting_clusters) {
    unsigned long rows = points.rows();
    unsigned long cols = points.cols();
    Matrix clusters(starting_clusters);
    Matrix temp_clusters(clusters);
    VectorXs counts(nclusters);
    VectorXs labels(rows);
    double error = 0.0;
    double old_error = 0.0;

#pragma omp parallel default(shared)
    {
        Matrix local_clusters(clusters);
        VectorXs local_counts(counts);
        do {
            for (size_t i = 0; i < nclusters; ++i) {
                local_counts[i] = 0;
                for (size_t j = 0; j < cols; ++j) {
                    local_clusters(i, j) = 0.0;
                }
            }
#pragma omp single
            {
                old_error = error;
                error = 0.0;
                for (size_t i = 0; i < nclusters; ++i) {
                    counts[i] = 0;
                    for (size_t j = 0; j < cols; ++j) {
                        temp_clusters(i, j) = 0.0;
                    }
                }
            }

#pragma omp for reduction(+ : error) nowait
            for (size_t i = 0; i < rows; ++i) {
                double min_distance = std::numeric_limits<double>::max();
                for (size_t j = 0; j < nclusters; ++j) {
                    double distance = 0;
                    for (size_t k = 0; k < cols; ++k) {
                        distance += std::pow(points(i, k) - clusters(j, k), 2);
                    }
                    if (distance < min_distance) {
                        labels[i] = j;
                        min_distance = distance;
                    }
                }

                for (size_t k = 0; k < cols; ++k) {
                    local_clusters(labels[i], k) += points(i, k);
                }
                ++local_counts[labels[i]];
                error += min_distance;
            }

#pragma omp critical
            for (size_t j = 0; j < nclusters; ++j) {
                counts[j] += local_counts[j];
                for (size_t k = 0; k < cols; ++k) {
                    temp_clusters(j, k) += local_clusters(j, k);
                }
            }

#pragma omp barrier
#pragma omp single
            for (size_t j = 0; j < nclusters; ++j) {
                for (size_t k = 0; k < cols; ++k) {
                    clusters(j, k) = counts[j] ? temp_clusters(j, k) / counts[j]
                                               : temp_clusters(j, k);
                }
            }
        } while (std::abs(error - old_error) > epsilon);
    }

    double max_radius = std::numeric_limits<double>::min();
    Vector radii =
        Vector::Constant(nclusters, std::numeric_limits<double>::min());
    for (size_t i = 0; i < rows; ++i) {
        double distance = 0.0;
        for (size_t k = 0; k < cols; ++k) {
            distance += std::pow(points(i, k) - clusters(labels[i], k), 2);
        }
        distance = std::sqrt(distance);
        if (distance > radii[labels[i]]) {
            radii[labels[i]] = distance;
        }
        if (distance > max_radius) {
            max_radius = distance;
        }
    }

    return {max_radius, labels, clusters, counts, radii};
}
}