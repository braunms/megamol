/*
 * AstroSchulz.h
 *
 * Copyright (C) 2019 by VISUS (Universitaet Stuttgart)
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOL_ASTRO_ASTROSCHULZ_H_INCLUDED
#define MEGAMOL_ASTRO_ASTROSCHULZ_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "astro/AstroDataCall.h"

#include "mmcore/CalleeSlot.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/Module.h"

#include "mmcore/moldyn/MultiParticleDataCall.h"

#include "mmcore/param/ParamSlot.h"

#include "mmstd_datatools/table/TableDataCall.h"


namespace megamol {
namespace astro {

    /// <summary>
    /// Converts from <see cref="AstroDataCall" /> to a table for data
    /// visualisation.
    /// </summary>
    class AstroSchulz : public core::Module {

    public:

        static inline const char *ClassName(void) {
            return "AstroSchulz"; 
        }

        static inline const char *Description(void) {
            return "Converts data contained in a AstroDataCall to a "
                "TableDataCall";
        }

        static bool IsAvailable(void) {
            return true;
        }

        /** Ctor. */
        AstroSchulz(void);

        /** Dtor. */
        virtual ~AstroSchulz(void);

    protected:

        virtual bool create(void);

        virtual void release(void);

    private:

        typedef megamol::stdplugin::datatools::table::TableDataCall::ColumnInfo
            ColumnInfo;

        static constexpr inline std::pair<float, float> initialiseRange(void) {
            return std::make_pair((std::numeric_limits<float>::max)(),
                std::numeric_limits<float>::lowest());
        }

        static void updateRange(std::pair<float, float>& range,
            const float value);

        void convert(float *dst, const std::size_t col, const vec3ArrayPtr& src);

        void convert(float *dst, const std::size_t col, const floatArrayPtr& src);

        void convert(float *dst, const std::size_t col, const boolArrayPtr& src);

        void convert(float *dst, const std::size_t col, const idArrayPtr& src);

        bool getData(core::Call& call);

        bool getHash(core::Call& call);

        void norm(float *dst, const std::size_t col, const vec3ArrayPtr& src);

        void setRange(const std::size_t col, const std::pair<float, float>& src);

        std::vector<ColumnInfo> columns;
        unsigned int frameID;
        std::size_t hash;
        core::param::ParamSlot paramIncrementalRange;
        std::vector<std::pair<float, float>> ranges;
        core::CallerSlot slotAstroData;
        core::CalleeSlot slotTableData;
        std::vector<float> values;
    };

} /* end namespace astro */
} /* end namespace megamol */

#endif /* MEGAMOL_ASTRO_ASTROSCHULZ_H_INCLUDED */