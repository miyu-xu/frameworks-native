#pragma once

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "utils.h"

namespace android {
namespace idlcli {

enum Status : unsigned int {
    OK,
    USAGE,
    UNAVAILABLE,
    ERROR,
};

class Args {
public:
    Args(const int argc, const char* const argv[]) {
        for (int argi = 0; argi < argc; argi++) {
            mArgs.emplace_back(std::string_view(argv[argi]));
        }
    }

    template <typename T = std::string>
    std::optional<T> get() {
        return get<T>(false);
    }

    template <typename T = std::string>
    std::optional<T> pop() {
        return get<T>(true);
    }

    bool empty() { return mArgs.empty(); }

private:
    template <typename T>
    std::optional<T> get(bool erase) {
        using idlcli::operator>>;
        using overrides::operator>>;
        T retValue;

        if (mArgs.empty()) {
            return {};
        }

        std::stringstream stream{std::string{mArgs.front()}};
        stream >> std::setbase(0) >> retValue;
        if (!stream || !stream.eof()) {
            return {};
        }

        if (erase) {
            mArgs.erase(mArgs.begin());
        }

        return retValue;
    }

    std::vector<std::string_view> mArgs;
};

class Command {
protected:
    struct Usage {
        std::string name;
        std::vector<std::string> details;
    };
    using UsageDetails = std::vector<Usage>;

public:
    virtual ~Command() = default;

    Status main(Args&& args) {
        Status status = doArgsAndMain(std::move(args));
        if (status == USAGE) {
            printUsage();
            return ERROR;
        }
        if (status == UNAVAILABLE) {
            std::cerr << "The requested operation is unavailable." << std::endl;
            return ERROR;
        }
        return status;
    }

private:
    virtual std::string getDescription() const = 0;
    virtual std::string getUsageSummary() const = 0;
    virtual UsageDetails getUsageDetails() const = 0;
    virtual Status doArgs(Args& args) = 0;
    virtual Status doMain(Args&& args) = 0;

    void printUsage() const {
        std::cerr << "Description:\n  " << getDescription() << std::endl;
        std::cerr << "Usage:\n  " << mName << " " << getUsageSummary() << std::endl;

        std::cerr << "Details:" << std::endl;
        size_t entryNameWidth = 0;
        for (auto& entry : getUsageDetails()) {
            entryNameWidth = std::max(entryNameWidth, entry.name.length());
        }
        for (auto& entry : getUsageDetails()) {
            auto prefix = entry.name;
            for (auto& line : entry.details) {
                std::cerr << "  " << std::left << std::setw(entryNameWidth + 8) << prefix << line
                          << std::endl;
                prefix = "";
            }
        }
    }

    Status doArgsAndMain(Args&& args) {
        Status status;
        mName = *args.pop();
        if ((status = doArgs(args)) != OK) {
            return status;
        }
        if ((status = doMain(std::move(args))) != OK) {
            return status;
        }
        return OK;
    }

protected:
    std::string mName;
};

template <typename T>
class CommandRegistry {
private:
    using CommandCreator = std::function<std::unique_ptr<Command>()>;

public:
    template <typename U>
    static CommandCreator Register(const std::string name) {
        Instance()->mCommands[name] = [] { return std::make_unique<U>(); };
        return Instance()->mCommands[name];
    }

    static std::unique_ptr<Command> Create(const std::string name) {
        auto it = Instance()->mCommands.find(name);
        if (it == Instance()->mCommands.end()) {
            return nullptr;
        }
        return it->second();
    }

    static auto List() {
        std::vector<std::string> list;
        for (auto& it : Instance()->mCommands) {
            list.push_back(it.first);
        }
        std::sort(list.begin(), list.end());
        return list;
    }

private:
    static CommandRegistry* Instance() {
        static CommandRegistry sRegistry;
        return &sRegistry;
    }

private:
    std::map<const std::string, CommandCreator> mCommands;
};

template <typename T>
class CommandWithSubcommands : public Command {
protected:
    Status doArgs(Args& args) override {
        mCommand = CommandRegistry<T>::Create(*args.get());
        if (!mCommand) {
            std::cerr << "Invalid Command!" << std::endl;
            return USAGE;
        }
        return OK;
    }

    Status doMain(Args&& args) override { return mCommand->main(std::move(args)); }

protected:
    std::unique_ptr<Command> mCommand;
};

} // namespace idlcli
} // namespace android
