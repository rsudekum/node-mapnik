#ifndef __NODE_MAPNIK_GRID_UTILS_H__
#define __NODE_MAPNIK_GRID_UTILS_H__

// mapnik
#include <mapnik/feature.hpp>           // for feature_impl, etc
#include <mapnik/grid/grid.hpp>         // for grid
#include <mapnik/version.hpp>           // for MAPNIK_VERSION

#include "utils.hpp"

// boost
#include <boost/foreach.hpp>

// stl
#include <cmath> // ceil
#include <stdint.h>  // for uint16_t

#if MAPNIK_VERSION >= 300000
#include <memory>
#else
#include <boost/shared_array.hpp>
#endif

using namespace v8;
using namespace node;

namespace node_mapnik {

#if MAPNIK_VERSION >= 300000
typedef std::unique_ptr<uint16_t[]> grid_line_type;
#else
typedef boost::shared_array<uint16_t> grid_line_type;
#endif

template <typename T>
static void grid2utf(T const& grid_type,
                     std::vector<grid_line_type> & lines,
                     std::vector<typename T::lookup_type>& key_order,
                     unsigned int resolution)
{
    typedef std::map< typename T::lookup_type, typename T::value_type> keys_type;
    typedef typename keys_type::const_iterator keys_iterator;

    typename T::feature_key_type const& feature_keys = grid_type.get_feature_keys();
    typename T::feature_key_type::const_iterator feature_pos;

    keys_type keys;
    // start counting at utf8 codepoint 32, aka space character
    uint16_t codepoint = 32;

    unsigned array_size = std::ceil(grid_type.width()/static_cast<float>(resolution));
    for (unsigned y = 0; y < grid_type.height(); y=y+resolution)
    {
        uint16_t idx = 0;
        grid_line_type line(new uint16_t[array_size]);
        typename T::value_type const* row = grid_type.getRow(y);
        for (unsigned x = 0; x < grid_type.width(); x=x+resolution)
        {
            // todo - this lookup is expensive
            typename T::value_type feature_id = row[x];
            feature_pos = feature_keys.find(feature_id);
            if (feature_pos != feature_keys.end())
            {
                typename T::lookup_type const& val = feature_pos->second;
                keys_iterator key_pos = keys.find(val);
                if (key_pos == keys.end())
                {
                    // Create a new entry for this key. Skip the codepoints that
                    // can't be encoded directly in JSON.
                    if (codepoint == 34) ++codepoint;      // Skip "
                    else if (codepoint == 92) ++codepoint; // Skip backslash
                    if (feature_id == mapnik::grid::base_mask)
                    {
                        keys[""] = codepoint;
                        key_order.push_back("");
                    }
                    else
                    {
                        keys[val] = codepoint;
                        key_order.push_back(val);
                    }
                    line[idx++] = static_cast<uint16_t>(codepoint);
                    ++codepoint;
                }
                else
                {
                    line[idx++] = static_cast<uint16_t>(key_pos->second);
                }
            }
            // else, shouldn't get here...
        }
#if MAPNIK_VERSION >= 300000
        lines.push_back(std::move(line));
#else
        lines.push_back(line);
#endif
    }
}


template <typename T>
static void write_features(T const& grid_type,
                           Local<Object>& feature_data,
                           std::vector<typename T::lookup_type> const& key_order)
{
    NanScope();
    typename T::feature_type const& g_features = grid_type.get_grid_features();
    if (g_features.size() <= 0)
    {
        return;
    }
    std::set<std::string> const& attributes = grid_type.property_names();
    typename T::feature_type::const_iterator feat_end = g_features.end();
    BOOST_FOREACH ( std::string const& key_item, key_order )
    {
        if (key_item.empty())
        {
            continue;
        }

        typename T::feature_type::const_iterator feat_itr = g_features.find(key_item);
        if (feat_itr == feat_end)
        {
            continue;
        }

        bool found = false;
        Local<Object> feat = NanNew<Object>();
        mapnik::feature_ptr feature = feat_itr->second;
        BOOST_FOREACH ( std::string const& attr, attributes )
        {
            if (attr == "__id__")
            {
                feat->Set(NanNew(attr.c_str()), NanNew<Integer>(feature->id()));
            }
            else if (feature->has_key(attr))
            {
                found = true;
                mapnik::feature_impl::value_type const& attr_val = feature->get(attr);
                feat->Set(NanNew(attr.c_str()),
                    boost::apply_visitor(node_mapnik::value_converter(),
                    attr_val.base()));
            }
        }

        if (found)
        {
            feature_data->Set(NanNew(feat_itr->first.c_str()), feat);
        }
    }
}

}
#endif // __NODE_MAPNIK_GRID_UTILS_H__
