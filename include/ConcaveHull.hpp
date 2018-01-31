//
// Created by jachu on 31.01.18.
//

#ifndef PLANELOC_CONCAVEHULL_HPP
#define PLANELOC_CONCAVEHULL_HPP

#include <vector>

#include <Eigen/Eigen>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/algorithm.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Alpha_shape_2.h>
#include <CGAL/IO/io.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Polyline_simplification_2/simplify.h>
#include <pcl/impl/point_types.hpp>
#include <pcl/point_cloud.h>
#include "Types.hpp"

class ConcaveHull {
    typedef CGAL::Exact_predicates_inexact_constructions_kernel  K;
    typedef K::FT                                                FT;
    typedef K::Point_2                                           Point;
    typedef K::Segment_2                                         Segment;
    typedef CGAL::Alpha_shape_vertex_base_2<K>                   Vb;
    typedef CGAL::Alpha_shape_face_base_2<K>                     Fb;
    typedef CGAL::Triangulation_data_structure_2<Vb,Fb>          Tds;
    typedef CGAL::Delaunay_triangulation_2<K,Tds>                Triangulation_2;
    typedef CGAL::Alpha_shape_2<Triangulation_2>                 Alpha_shape_2;
    typedef Alpha_shape_2::Alpha_shape_edges_iterator            Alpha_shape_edges_iterator;
    typedef Alpha_shape_2::Alpha_shape_vertices_iterator         Alpha_shape_vertices_iterator;
    typedef CGAL::Polygon_2<K>                                   Polygon_2;
    typedef CGAL::Polygon_with_holes_2<K>                        Polygon_holes_2;
    typedef CGAL::Polyline_simplification_2::Stop_above_cost_threshold Stop;
    typedef CGAL::Polyline_simplification_2::Squared_distance_cost     Cost;
    
public:
    
    ConcaveHull(pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr ipoints3d,
                Eigen::Vector4d planeEq);
    
    ConcaveHull(const std::vector<Polygon_2> &polygons,
                const std::vector<pcl::PointCloud<pcl::PointXYZRGB>::Ptr> &polygons3d,
                const Eigen::Vector3d &plNormal,
                double plD);
    
    const std::vector<pcl::PointCloud<pcl::PointXYZRGB>::Ptr> &getPolygons3d() const {
        return polygons3d;
    }
    
    const std::vector<double> &getAreas() const {
        return areas;
    }
    
    double getTotalArea() const {
        return totalArea;
    }

    ConcaveHull intersect(const ConcaveHull &other) const;
    
    ConcaveHull transform(Vector7d transform) const;
    
private:
    
    void computeFrame();
    
    Point point3dTo2d(const Eigen::Vector3d &point3d) const;
    
    Eigen::Vector3d point2dTo3d(const Point &point2d) const;
    
    std::vector<Polygon_2> polygons;
    std::vector<double> areas;
    double totalArea;
    std::vector<pcl::PointCloud<pcl::PointXYZRGB>::Ptr> polygons3d;
    
    Eigen::Vector3d plNormal;
    double plD;
    // point on the plane nearest to origin
    Eigen::Vector3d origin;
    Eigen::Vector3d xAxis, yAxis;
};


#endif //PLANELOC_CONCAVEHULL_HPP