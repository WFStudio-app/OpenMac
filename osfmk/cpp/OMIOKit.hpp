/*
 * OMIOKit.hpp — IOKit-flavoured device-tree layer for OpenMac.
 * Original code; reproduces only the public shape of IOKit concepts
 * (registry entries, matching) over Linux sysfs. Proprietary license.
 */
#ifndef _OPENMAC_CPP_OMIOKIT_HPP
#define _OPENMAC_CPP_OMIOKIT_HPP

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>

namespace omac { namespace iokit {

struct RegistryEntry {
    std::string className;                       /* e.g. IOPCIDevice      */
    std::string identifier;                      /* e.g. PCI vendor/device*/
    std::string path;                            /* sysfs source path     */
    std::map<std::string, std::string> props;    /* property table        */
};

inline std::string readFile(const std::string &p) {
    std::ifstream f(p);
    if (!f) return {};
    std::ostringstream ss; ss << f.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back()=='\n' || s.back()=='\r')) s.pop_back();
    return s;
}

class Registry {
public:
    /* Scan sysfs and build a Darwin-style registry snapshot. */
    static Registry scan() {
        Registry r;
        /* CPU nodes -> AppleACPICPU-flavoured class names */
        DIR *d = opendir("/sys/devices/system/cpu");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                std::string n = e->d_name;
                if (n.rfind("cpu", 0) == 0 && n.size() <= 6 &&
                    n.find_first_of("0123456789", 3) != std::string::npos) {
                    RegistryEntry en;
                    en.className = "IOPowerManagementCPU";
                    en.identifier = n;
                    en.path = "/sys/devices/system/cpu/" + n;
                    en.props["cpufreq"] = readFile(
                        en.path + "/cpufreq/scaling_governor");
                    en.props["online"] = readFile(en.path + "/online");
                    r.entries_.push_back(std::move(en));
                }
            }
            closedir(d);
        }
        /* PCI devices -> IOPCIDevice */
        d = opendir("/sys/bus/pci/devices");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                std::string base = std::string("/sys/bus/pci/devices/") + e->d_name;
                RegistryEntry en;
                en.className = "IOPCIDevice";
                en.identifier = e->d_name;
                en.path = base;
                en.props["vendor"]   = readFile(base + "/vendor");
                en.props["device"]   = readFile(base + "/device");
                en.props["class"]    = readFile(base + "/class");
                r.entries_.push_back(std::move(en));
            }
            closedir(d);
        }
        /* Block devices -> IOMedia / AppleDiskImagesInterface */
        d = opendir("/sys/block");
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                std::string base = std::string("/sys/block/") + e->d_name;
                RegistryEntry en;
                en.className = "IOMedia";
                en.identifier = e->d_name;
                en.path = base;
                en.props["size"]     = readFile(base + "/size");
                en.props["rotational"] = readFile(base + "/queue/rotational");
                r.entries_.push_back(std::move(en));
            }
            closedir(d);
        }
        return r;
    }

    const std::vector<RegistryEntry>& entries() const { return entries_; }

    std::vector<const RegistryEntry*> matching(const std::string &className) const {
        std::vector<const RegistryEntry*> out;
        for (auto &e : entries_)
            if (e.className == className) out.push_back(&e);
        return out;
    }

private:
    std::vector<RegistryEntry> entries_;
};

}} /* namespace omac::iokit */

#endif
