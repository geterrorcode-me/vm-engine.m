#ifndef VMEER_WMS_RUNTIME_H
#define VMEER_WMS_RUNTIME_H

#include <string>

namespace vmeer {
namespace wms {
    void patch_window_layout(void* layout_params_parcel);
    void sanitize_window_title(std::string& title);
}
}

#endif
