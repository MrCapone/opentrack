#pragma once
/* Copyright (c) 2013 Stanislaw Halik <sthalik@misaki.pl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include "ui_ftnoir_kalman_filtercontrols.h"
#include "opentrack/plugin-api.hpp"
#include "opentrack-compat/options.hpp"
using namespace options;
#include "opentrack-compat/timer.hpp"

#include <Eigen/Core>
#include <Eigen/LU>

#include <QString>
#include <QWidget>

#include <atomic>

static constexpr int NUM_STATE_DOF = 12;
static constexpr int NUM_MEASUREMENT_DOF = 6;
// These vectors are compile time fixed size, stack allocated
using StateToMeasureMatrix = Eigen::Matrix<double, NUM_MEASUREMENT_DOF, NUM_STATE_DOF>;
using StateMatrix = Eigen::Matrix<double, NUM_STATE_DOF, NUM_STATE_DOF>;
using MeasureToStateMatrix = Eigen::Matrix<double, NUM_STATE_DOF, NUM_MEASUREMENT_DOF>;
using MeasureMatrix = Eigen::Matrix<double, NUM_MEASUREMENT_DOF, NUM_MEASUREMENT_DOF>;
using StateVector = Eigen::Matrix<double, NUM_STATE_DOF, 1>;
using PoseVector = Eigen::Matrix<double, NUM_MEASUREMENT_DOF, 1>;

struct KalmanFilter
{
    MeasureMatrix
        measurement_noise_cov;
    StateMatrix
        process_noise_cov,
        state_cov,
        state_cov_prior,
        transition_matrix;
    MeasureToStateMatrix
        kalman_gain;
    StateToMeasureMatrix
        measurement_matrix;
    StateVector
        state,
        state_prior;
    PoseVector
        innovation;
    void init();
    void time_update();
    void measurement_update(const PoseVector &measurement);

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

struct KalmanProcessNoiseScaler
{
    MeasureMatrix
        innovation_cov_estimate;
    StateMatrix
        base_cov; // baseline (unscaled) process noise covariance matrix
    void init();
    void update(KalmanFilter &kf, double dt);

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};


struct DeadzoneFilter
{
    PoseVector
        last_output,
        dz_size;
    DeadzoneFilter() :
        last_output(PoseVector::Zero()),
        dz_size(PoseVector::Zero())
    {}
    void reset() {
        last_output = PoseVector::Zero();
    }
    PoseVector filter(const PoseVector &input);

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};


struct settings : opts {
    value<slider_value> noise_rot_slider_value;
    value<slider_value> noise_pos_slider_value;

    static constexpr double adaptivity_window_length = 0.5; // seconds
    static constexpr double deadzone_scale = 2.;
    static constexpr double deadzone_exponent = 4.0;
    static constexpr double process_sigma_pos = 0.5;
    static constexpr double process_simga_rot = 0.5;

    static double map_slider_value(const slider_value &v)
    {
        //return std::pow(4., v * 5. - 4.) / 4. * 10.; // not so much difference, except that it is harder to adjust the min-max range.
        return std::pow(10., v * 4. - 3.);
    }

    settings() :
        opts("kalman-filter"),
        noise_rot_slider_value(b, "noise-rotation-slider", slider_value(0.5, 0., 1.)),
        noise_pos_slider_value(b, "noise-position-slider", slider_value(0.5, 0., 1.))
    {}

};

class FTNoIR_Filter : public IFilter
{
    PoseVector do_kalman_filter(const PoseVector &input, double dt, bool new_input);
    void fill_transition_matrix(double dt);
    void fill_process_noise_cov_matrix(StateMatrix &target, double dt) const;
public:
    FTNoIR_Filter();
    void reset();
    void filter(const double *input, double *output);
    PoseVector last_input;
    Timer timer;
    bool first_run;
    double dt_since_last_input;
    settings s;
    KalmanFilter kf;
    KalmanProcessNoiseScaler kf_adaptive_process_noise_cov;
    //PoseVector minimal_state_var;
    DeadzoneFilter dz_filter;
    slider_value prev_slider_pos[2];

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

class FTNoIR_FilterDll : public Metadata
{
public:
    QString name() { return QString("Kalman"); }
    QIcon icon() { return QIcon(":/images/filter-16.png"); }
};

class FilterControls: public IFilterDialog
{
    Q_OBJECT
public:
    FilterControls();
    Ui::KalmanUICFilterControls ui;
    void register_filter(IFilter*) override {}
    void unregister_filter() override {}
    settings s;
    FTNoIR_Filter *filter;
public slots:
    void doOK();
    void doCancel();
    void updateLabels(const slider_value&);
};
