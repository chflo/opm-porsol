//===========================================================================
//
// File: MiscibilityWater.hpp
//
// Created: Tue May 18 10:26:13 2010
//
// Author(s): Atgeirr F Rasmussen <atgeirr@sintef.no>
//            Bj�rn Spjelkavik    <bsp@sintef.no>
//
// $Date$
//
// $Revision$
//
//===========================================================================
/*
  Copyright 2010 SINTEF ICT, Applied Mathematics.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPENRS_MISCIBILITYWATER_HEADER
#define OPENRS_MISCIBILITYWATER_HEADER

#include "MiscibilityProps.hpp"
#include <opm/common/ErrorMacros.hpp>

#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/Deck/DeckItem.hpp>

// Forward declaration.
class PVTW;

namespace Opm
{
    class MiscibilityWater : public MiscibilityProps
    {
    public:
        typedef std::vector<std::vector<double> > table_t;

        MiscibilityWater(const DeckKeyword& pvtwKeyword)
        {
            const auto& pvtwRecord = pvtwKeyword.getRecord(0);
            ref_press_ = pvtwRecord.getItem("P_REF").getSIDouble(0);
            ref_B_     = pvtwRecord.getItem("WATER_VOL_FACTOR").getSIDouble(0);
            comp_      = pvtwRecord.getItem("WATER_COMPRESSIBILITY").getSIDouble(0);
            viscosity_ = pvtwRecord.getItem("WATER_VISCOSITY").getSIDouble(0);

            if (pvtwRecord.getItem("WATER_VISCOSIBILITY").getSIDouble(0) != 0.0) {
                OPM_THROW(std::runtime_error, "MiscibilityWater does not support 'viscosibility'.");
            }
        }


        MiscibilityWater(double visc)
            : ref_press_(0.0),
              ref_B_(1.0),
              comp_(0.0),
              viscosity_(visc)
        {
        }

        // WTF?? we initialize a class for water from a keyword for oil?
        void initFromPvcdo(const DeckKeyword& pvcdoKeyword)
        {
            const auto& pvcdoRecord = pvcdoKeyword.getRecord(0);
            ref_press_ = pvcdoRecord.getItem("P_REF").getSIDouble(0);
            ref_B_     = pvcdoRecord.getItem("OIL_VOL_FACTOR").getSIDouble(0);
            comp_      = pvcdoRecord.getItem("OIL_COMPRESSIBILITY").getSIDouble(0);
            viscosity_ = pvcdoRecord.getItem("OIL_VISCOSITY").getSIDouble(0);
            if (pvcdoRecord.getItem("OIL_VISCOSIBILITY").getSIDouble(0) != 0.0) {
                OPM_THROW(std::runtime_error, "MiscibilityWater does not support 'viscosibility'.");
            }
        }

	virtual ~MiscibilityWater()
        {
        }

        virtual double getViscosity(int /*region*/, double /*press*/, const surfvol_t& /*surfvol*/) const
        {
            return viscosity_;
        }

        virtual void getViscosity(const std::vector<PhaseVec>& pressures,
                                  const std::vector<CompVec>&,
                                  int,
                                  std::vector<double>& output) const
        {
            int num = pressures.size();
            output.clear();
            output.resize(num, viscosity_);
        }

        virtual double B(int /*region*/, double press, const surfvol_t& /*surfvol*/) const
        {
            if (comp_) {
                // Computing a polynomial approximation to the exponential.
                double x = comp_*(press - ref_press_);
                return ref_B_/(1.0 + x + 0.5*x*x);
            } else {
                return ref_B_;
            }
        }

        virtual void B(const std::vector<PhaseVec>& pressures,
                       const std::vector<CompVec>&,
                       int phase,
                       std::vector<double>& output) const
        {
            int num = pressures.size();
            if (comp_) {
                output.resize(num);
#pragma omp parallel for
                for (int i = 0; i < num; ++i) {
                    // Computing a polynomial approximation to the exponential.
                    double x = comp_*(pressures[i][phase] - ref_press_);
                    output[i] = ref_B_/(1.0 + x + 0.5*x*x);
                }
            } else {
                output.clear();
                output.resize(num, ref_B_);
            }
        }

	virtual double dBdp(int region, double press, const surfvol_t& surfvol) const
        {
            if (comp_) {
                return -comp_*B(region, press, surfvol);
            } else {
                return 0.0;
            }
        }

        virtual void dBdp(const std::vector<PhaseVec>& pressures,
                          const std::vector<CompVec>& surfvols,
                          int phase,
                          std::vector<double>& output_B,
                          std::vector<double>& output_dBdp) const
        {
            B(pressures, surfvols, phase, output_B);
            int num = pressures.size();
            if (comp_) {
                output_dBdp.resize(num);
#pragma omp parallel for
                for (int i = 0; i < num; ++i) {
                    output_dBdp[i] = -comp_*output_B[i];
                }
            } else {
                output_dBdp.clear();
                output_dBdp.resize(num, 0.0);
            }
        }

	virtual double R(int /*region*/, double /*press*/, const surfvol_t& /*surfvol*/) const
        {
            return 0.0;
        }

        virtual void R(const std::vector<PhaseVec>& pressures,
                       const std::vector<CompVec>&,
                       int,
                       std::vector<double>& output) const
        {
            int num = pressures.size();
            output.clear();
            output.resize(num, 0.0);
        }

	virtual double dRdp(int /*region*/, double /*press*/, const surfvol_t& /*surfvol*/) const
        {
            return 0.0;
        }

        virtual void dRdp(const std::vector<PhaseVec>& pressures,
                          const std::vector<CompVec>&,
                          int, 
                          std::vector<double>& output_R,
                          std::vector<double>& output_dRdp) const
        {
            int num = pressures.size();
            output_R.clear();
            output_R.resize(num, 0.0);
            output_dRdp.clear();
            output_dRdp.resize(num, 0.0);
        }

    private:
        double ref_press_;
        double ref_B_;
        double comp_;
        double viscosity_;
    };

}

#endif // OPENRS_MISCIBILITYWATER_HEADER
