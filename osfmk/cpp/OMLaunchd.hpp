/*
 * OMLaunchd.hpp — launchd-flavoured job manager (C++ module).
 * Manages plist-like job definitions in $HOME/.omac/LaunchDaemons style
 * directories; starts/stops tracked children. Original code, proprietary.
 */
#ifndef _OPENMAC_CPP_OMLAUNCHD_HPP
#define _OPENMAC_CPP_OMLAUNCHD_HPP

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace omac { namespace launchd {

struct Job {
    std::string label;
    std::string program;
    std::vector<std::string> args;
    bool keepAlive = false;
    pid_t pid = 0;               /* live pid if started */
};

class Manager {
public:
    explicit Manager(std::string stateDir) : stateDir_(std::move(stateDir)) {
        std::string cmd = "mkdir -p '" + stateDir_ + "'";
        if (system(cmd.c_str()) != 0) { /* best effort */ }
    }

    /* Parse a plist-like job file: Label=, Program=, ProgramArguments=a b c, KeepAlive= */
    static Job parseFile(const std::string &path) {
        Job j;
        std::ifstream f(path);
        std::string line;
        while (std::getline(f, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = trim(line.substr(0, eq));
            std::string v = trim(line.substr(eq + 1));
            if (k == "Label") j.label = v;
            else if (k == "Program") j.program = v;
            else if (k == "ProgramArguments") splitWs(v, j.args);
            else if (k == "KeepAlive") j.keepAlive = (v=="true"||v=="1"||v=="yes");
        }
        if (j.label.empty()) {
            auto s = path.rfind('/');
            j.label = (s==std::string::npos?path:path.substr(s+1));
        }
        return j;
    }

    bool start(Job &j) {
        if (isRunning(j)) return true;
        pid_t p = fork();
        if (p < 0) return false;
        if (p == 0) {
            setsid();
            std::string exe = j.program.empty()
                ? (j.args.empty() ? "/bin/true" : j.args[0]) : j.program;
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(exe.c_str()));
            for (size_t i = j.program.empty()?1:0; i < j.args.size(); ++i)
                argv.push_back(const_cast<char*>(j.args[i].c_str()));
            argv.push_back(nullptr);
            execvp(exe.c_str(), argv.data());
            _exit(127);
        }
        j.pid = p;
        writeFile(pidPath(j.label), std::to_string(p));
        return true;
    }

    bool stop(Job &j) {
        pid_t p = j.pid ? j.pid : readPid(j.label);
        if (p > 0) kill(p, SIGTERM);
        j.pid = 0;
        unlink(pidPath(j.label).c_str());
        return true;
    }

    bool isRunning(const Job &j) {
        pid_t p = j.pid ? j.pid : readPid(j.label);
        if (p <= 0) return false;
        if (kill(p, 0) == 0) return true;
        return false;
    }

    std::vector<Job> loadAll(const std::string &dir) {
        std::vector<Job> out;
        DIR *d = opendir(dir.c_str());
        if (!d) return out;
        struct dirent *e;
        while ((e = readdir(d))) {
            std::string n = e->d_name;
            if (n.size() > 5 && n.compare(n.size()-5,5,".conf")==0) {
                Job j = parseFile(dir + "/" + n);
                jobs_[j.label] = j;
                out.push_back(jobs_[j.label]);
            }
        }
        closedir(d);
        return out;
    }

private:
    std::string stateDir_;
    std::map<std::string, Job> jobs_;

    std::string pidPath(const std::string &label) const {
        return stateDir_ + "/" + label + ".pid";
    }
    pid_t readPid(const std::string &label) {
        std::ifstream f(pidPath(label));
        pid_t p = 0; if (f) f >> p; return p;
    }
    static void writeFile(const std::string &p, const std::string &v) {
        std::ofstream f(p); f << v;
    }
    static std::string trim(std::string s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        return a==std::string::npos ? "" : s.substr(a, b-a+1);
    }
    static void splitWs(const std::string &s, std::vector<std::string> &out) {
        std::istringstream is(s); std::string w;
        while (is >> w) out.push_back(w);
    }
};

}} /* namespace omac::launchd */

#endif
