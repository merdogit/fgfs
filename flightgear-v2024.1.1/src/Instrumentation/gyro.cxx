// gyro.cxx - simple implementation of a spinning gyro model.

// Todo: this should be really modelled physically correctly and initialize its own properties etc.
//       this way, instruments using a gyro can just refer to it and read the data from the gyro.

#include "gyro.hxx"

Gyro::Gyro ()
    : _serviceable(true),
      _power_norm(0.0),
      _spin_norm(0.0)
{
}

Gyro::~Gyro ()
{
}

void Gyro::reinit(void)
{
    _power_norm = 0.0;
    _spin_norm = 0.0;
    _spin_down = 180.0; // about 3 minutes from full spin
    _spin_up = 4.0;     // up to power-norm; about 4 seconds to full spin
}

void
Gyro::update (double delta_time_sec)
{
                                // spin decays 0.5% every second
                                double spin_decay = (1.0 / _spin_down) * delta_time_sec;
                                _spin_norm -= spin_decay;

                                // power can increase spin by 25%
                                // every second, but only up to the
                                // level of power available
    if (_serviceable) {
        double step = spin_decay + (1.0 / _spin_up) * _power_norm * delta_time_sec;
        if ((_spin_norm + step) <= _power_norm)
            _spin_norm += step;
    } else {
        _spin_norm = 0;         // stop right away if the gyro breaks
    }

                                // clamp the spin to 0.0:1.0
    if (_spin_norm < 0.0)
        _spin_norm = 0.0;
    else if (_spin_norm > 1.0)
        _spin_norm = 1.0;
}

void
Gyro::set_power_norm (double power_norm)
{
    _power_norm = power_norm;
}

double
Gyro::get_spin_norm () const
{
    return _spin_norm;
}

void
Gyro::set_spin_norm (double spin_norm)
{
    _spin_norm = spin_norm;
}

bool
Gyro::is_serviceable () const
{
    return _serviceable;
}

void
Gyro::set_serviceable (bool serviceable)
{
    _serviceable = serviceable;
}

void Gyro::set_spin_up(double spin_up)
{
    _spin_up = spin_up;
}

void Gyro::set_spin_down(double spin_down)
{
    _spin_down = spin_down;
}


// end of gyro.cxx

