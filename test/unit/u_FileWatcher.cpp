#include "catch_wrapper.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <limits.h>
#include <locale>
#include <string>
#include <thread>

#include "pfw/FileSystemWatcher.h"

#include "testutil/FileSandbox.h"
#include "testutil/test_helper.h"

using namespace std::chrono_literals;
using namespace pfw;

#if defined(__APPLE__)
static constexpr const int                       grace_period_ms = 2050;
static constexpr const std::chrono::milliseconds defaultLatency  = 1000ms;
#else
static constexpr const int                       grace_period_ms = 100;
static constexpr const std::chrono::milliseconds defaultLatency  = 20ms;
#endif

std::string mapToString(EventType type)
{
    std::string result = "(";

    if (created(type)) {
        result.append("created, ");
    }
    if (modified(type)) {
        result.append("modified, ");
    }
    if (deleted(type)) {
        result.append("deleted, ");
    }
    if (renamed(type)) {
        result.append("renamed, ");
    }
    if (buffer_overflow(type)) {
        result.append("buffer overflow, ");
    }
    if (failed(type)) {
        result.append("failed");
    }

    result.append(")");

    return result;
}

std::string eventToString(const Event &event)
{
    return "Event(Type: '" + mapToString(event.type) + "', relativePath: '" +
           event.relativePath.string() + "')";
}

using VecEvents = std::unique_ptr<std::vector<EventPtr>>;

class ExpectedEvent
{
  public:
    ExpectedEvent(const fs::path &relativePath,
                  EventType       flagSet,
                  EventType       flagNotSet = EventType::NOOP)
        : relativePath(relativePath)
        , mflagSet(flagSet)
        , mflagNotSet(flagNotSet)
    {
        if (!buffer_overflow(mflagSet)) {
            mflagNotSet = mflagNotSet | EventType::BUFFER_OVERFLOW;
        }
        if (!failed(mflagSet)) {
            mflagNotSet = mflagNotSet | EventType::FAILED;
        }
    }

    EventType mflagSet;
    EventType mflagNotSet;

    fs::path relativePath;
};

class TestFileSystemAdapter
{
  public:
    TestFileSystemAdapter(const fs::path &          path,
                          std::chrono::milliseconds duration)
        : vecEvents(new std::vector<EventPtr>())
        , fswatch(path,
                  duration,
                  std::bind(&TestFileSystemAdapter::listernerFunction,
                            this,
                            std::placeholders::_1))
    {
    }

    VecEvents getEventsAfterWait(std::chrono::microseconds ms)
    {
        std::this_thread::sleep_for(ms);
        std::lock_guard<std::mutex> lock(VecEvents);
        auto                        retVal = std::move(vecEvents);
        vecEvents.reset(new std::vector<EventPtr>());
        for (auto &event : *retVal) {
            std::cout << "received in test: '" << eventToString(*event) << "'"
                      << std::endl;
        }
        return retVal;
    }

    bool isWatching() { return fswatch.isWatching(); }

  private:
    void listernerFunction(std::vector<EventPtr> &&events)
    {
        std::lock_guard<std::mutex> lock(VecEvents);
        if (events.empty())
            return;
        for (auto &&event : events) {
            vecEvents->emplace_back(std::move(event));
        }
    }

    std::mutex        vecEventsMutex;
    VecEvents         vecEvents;
    FileSystemWatcher fswatch;
};

using TestFileSystemAdapterPtr = std::shared_ptr<TestFileSystemAdapter>;

TEST_CASE("test the file system watcher", "[FileSystemWatcher]")
{
    FileSandbox sandbox;

    fs::path relWatchedDir = "watched";
    fs::path absWatchedDir = sandbox.path() / relWatchedDir;
    sandbox.createDirectory(relWatchedDir);
    CHECK(fs::exists(absWatchedDir));

    // helper methods
    auto startWatching = [&](const std::chrono::milliseconds &latency =
                                 defaultLatency) {
        std::chrono::milliseconds mLatency =
            latency < defaultLatency ? defaultLatency : latency;
        auto watcher =
            std::make_shared<TestFileSystemAdapter>(absWatchedDir, mLatency);

        // lets give the file watcher some time to initialize, otherwise it is
        // possible, that some changes inside of the test will be missed
        std::this_thread::sleep_for(10ms);

        return watcher;
    };

    auto comparison = [](const Event &lhs, const ExpectedEvent &rhs) {
        if (lhs.relativePath != rhs.relativePath) {
            return false;
        }
        return (lhs.type & rhs.mflagSet) == rhs.mflagSet &&
               (~lhs.type & rhs.mflagNotSet) == rhs.mflagNotSet;
    };

    auto eventWasDetected =
        [comparison](TestFileSystemAdapterPtr &        testWatcher,
                     const std::vector<ExpectedEvent> &expectedEvents,
                     const std::vector<fs::path> &     noEvents = {}) -> bool {
        auto events = testWatcher->getEventsAfterWait(
            std::chrono::milliseconds(grace_period_ms));

        for (auto &expectedEvent : expectedEvents) {
            bool foundEvent = false;
            for (auto &event : *events) {
                if (comparison(*event, expectedEvent)) {
                    foundEvent = true;
                    break;
                }
            }
            if (!foundEvent) {
                return foundEvent;
            }
        }

        for (auto &noEvent : noEvents) {
            bool foundEvent = false;
            for (auto &event : *events) {
                if (event->relativePath == noEvent) {
                    foundEvent = true;
                    break;
                }
            }
            if (foundEvent) {
                return false;
            }
        }

        return true;
    };

    auto creatingFilesAsynchron = [&]() {
        return std::thread([&]() {
            for (size_t i = 0; i < 20; ++i) {
                fs::path fileName = "created_file_" + std::to_string(i);
                sandbox.createFile(relWatchedDir / fileName);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    };

    SECTION("calling dtor of filewatcher")
    {
        auto watcher = startWatching();
        REQUIRE_NOTHROW(watcher.reset());
    }

    SECTION("dtor during things happen on filesystem")
    {
        for (size_t i = 0; i < 20; ++i) {
            auto watcher = startWatching();

            auto thread = creatingFilesAsynchron();

            REQUIRE_NOTHROW(watcher.reset());

            thread.join();
        }
    }

    SECTION("directory does not exist")
    {
        sandbox.remove(relWatchedDir);
        auto watcher = startWatching();

        auto            t = fs::path("test");
        std::error_code ec;
        fs::canonical(t, ec);

        std::vector<ExpectedEvent> expectedEvents = {ExpectedEvent(
            fs::path("Failed to open directory."), EventType::FAILED,
            EventType::CREATED | EventType::MODIFIED | EventType::DELETED |
                EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));

        CHECK(!watcher->isWatching());
    }

    SECTION("file creation")
    {
        auto watcher = startWatching();

        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / fileName);

        std::vector<ExpectedEvent> expectedEvents = {ExpectedEvent(
            fileName, EventType::CREATED,
            EventType::MODIFIED | EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

#ifndef __APPLE__
    // disabled under MacOS because under HFS+ composed file name are not
    // valid. With APFS it works.
    SECTION("file creation unicode composed")
    {
        auto watcher = startWatching();

        fs::path fileName = fs::path(u8"éÄÅꜾПԵé.ꝋяզ");
        sandbox.createFile(relWatchedDir / fileName);

        std::vector<ExpectedEvent> expectedEvents = {ExpectedEvent(
            fileName, EventType::CREATED,
            EventType::MODIFIED | EventType::DELETED | EventType::RENAMED)};

        CHECK(watcher->isWatching());
        CHECK(eventWasDetected(watcher, expectedEvents));
    }
#endif

    SECTION("file creation unicode decomposed")
    {
        auto watcher = startWatching();

        fs::path fileName = "cra\u00A8ted_file";
        sandbox.createFile(relWatchedDir / fileName);

        std::vector<ExpectedEvent> expectedEvents = {ExpectedEvent(
            fileName, EventType::CREATED,
            EventType::MODIFIED | EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("file modification")
    {
        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / fileName);

        auto watcher = startWatching();
        sandbox.modifyFile(relWatchedDir / fileName, "content");

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::MODIFIED,
                          EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

#ifndef __LINUX__
    // linux has nearly always a case-sensitive file system
    SECTION("file modification (upper case lower case)")
    {
        fs::path fileName      = "created_file";
        fs::path otherfileName = "CrEaTeD_FiLe";
        auto     path          = sandbox.createFile(relWatchedDir / fileName);

        auto watcher = startWatching();
        sandbox.modifyFile(relWatchedDir / otherfileName, "content");

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::MODIFIED,
                          EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }
#endif

#ifndef __APPLE__
    // disabled under MacOS because under HFS+ composed file name are not
    // valid. With APFS it works.
    SECTION("file modification unicode composed")
    {
        fs::path fileName = fs::path(u8"éÄÅꜾПԵé.ꝋяզ");
        sandbox.createFile(relWatchedDir / fileName);

        auto watcher = startWatching();
        sandbox.modifyFile(relWatchedDir / fileName, "content");

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::MODIFIED,
                          EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }
#endif

    SECTION("file creation unicode decomposed")
    {
        fs::path fileName = "cra\u00A8ted_file";
        sandbox.createFile(relWatchedDir / fileName);

        auto watcher = startWatching();
        sandbox.modifyFile(relWatchedDir / fileName, "content");

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::MODIFIED,
                          EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("file deletion")
    {
        fs::path fileName = u8"created_file";
        sandbox.createFile(relWatchedDir / fileName);

        auto watcher = startWatching();

        sandbox.remove(relWatchedDir / fileName);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::DELETED,
                          EventType::MODIFIED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("file deletion unicode case remove decomposed")
    {
        fs::path fileNameDecomposed = "cra\u00A8ted_file";
        fs::path absFilePath =
            sandbox.createFile(relWatchedDir / fileNameDecomposed);

        auto watcher = startWatching();

        sandbox.remove(relWatchedDir / fileNameDecomposed);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileNameDecomposed, EventType::DELETED,
                          EventType::MODIFIED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("file deletion unicode case remove composed")
    {
        fs::path fileNameComposed = fs::path(u8"éÄÅꜾПԵé.ꝋяզ");
        fs::path relPath          = relWatchedDir / fileNameComposed;
        fs::path absFilePath      = sandbox.createFile(relPath);

        auto watcher = startWatching();

        sandbox.remove(relWatchedDir / fileNameComposed);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileNameComposed, EventType::DELETED,
                          EventType::MODIFIED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

#ifndef __LINUX__
    // linux is by default case sensitive. Because of that this test will
    // not works under linux
    SECTION("file deletion case upper case")
    {
        fs::path fileNameDown = u8"created_file";
        fs::path fileNameUp   = u8"CREATED_FILE";
        fs::path absFilePath = sandbox.createFile(relWatchedDir / fileNameDown);

        auto watcher = startWatching();

        sandbox.remove(relWatchedDir / fileNameUp);

        std::vector<ExpectedEvent> expectedEvents;

#if defined(__APPLE__)
        // from my point of view this is an bug inside of the apple api. But
        // apple does not want to fix that.
        // source: https://forums.developer.apple.com/thread/103108
        expectedEvents = {
            ExpectedEvent(fileNameUp, EventType::DELETED,
                          EventType::MODIFIED | EventType::RENAMED)};
#else
        expectedEvents           = {ExpectedEvent(
            fileNameDown, EventType::DELETED,
            EventType::CREATED | EventType::MODIFIED | EventType::RENAMED)};
#endif

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }
#endif

#ifndef __APPLE__
    // disabled under MacOS because under HFS+ composed file name are not
    // valid. With APFS it works.
    SECTION("add file in unicode directory composed")
    {
#ifndef __LINUX__
        fs::path dirNameComposed = fs::path(L"fold\u00E4");
#else
        fs::path dirNameComposed = fs::path(u8"foldä");
#endif
        sandbox.createDirectory(relWatchedDir / dirNameComposed);

        auto watcher = startWatching();

        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / dirNameComposed / fileName);

        std::vector<ExpectedEvent> expectedEvents = {ExpectedEvent(
            dirNameComposed / fileName, EventType::CREATED,
            EventType::MODIFIED | EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }
#endif

    SECTION("add file in unicode directory decomposed")
    {
#ifndef __LINUX__
        fs::path dirNameDecomposed = fs::path(L"folda\u00A8");
#else
        fs::path dirNameDecomposed = fs::path(u8"folda¨");
#endif
        sandbox.createDirectory(relWatchedDir / dirNameDecomposed);

        auto watcher = startWatching();

        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / dirNameDecomposed / fileName);

        std::vector<ExpectedEvent> expectedEvents = {ExpectedEvent(
            dirNameDecomposed / fileName, EventType::CREATED,
            EventType::MODIFIED | EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("file creation, modification, deletion and creation")
    {
        auto watcher = startWatching(40ms);

        fs::path fileName = "created_file";

        sandbox.createFile(relWatchedDir / fileName);
        sandbox.modifyFile(relWatchedDir / fileName, "content");
        sandbox.remove(relWatchedDir / fileName);
        sandbox.createFile(relWatchedDir / fileName);

        std::vector<ExpectedEvent> expectedEvents = {ExpectedEvent(
            fileName,
            EventType::CREATED | EventType::MODIFIED | EventType::DELETED,
            EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("file creation, modification, deletion and creation with "
            "waiting steps")
    {
        std::vector<ExpectedEvent> expectedEvents;

        auto watcher = startWatching(20ms);

        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / fileName);

        expectedEvents = {ExpectedEvent(
            fileName, EventType::CREATED,
            EventType::MODIFIED | EventType::DELETED | EventType::RENAMED)};
        REQUIRE(eventWasDetected(watcher, expectedEvents));

        sandbox.modifyFile(relWatchedDir / fileName, "content");

        expectedEvents = {
            ExpectedEvent(fileName, EventType::MODIFIED,
                          EventType::DELETED | EventType::RENAMED)};
        REQUIRE(eventWasDetected(watcher, expectedEvents));

        sandbox.remove(relWatchedDir / fileName);

        expectedEvents = {
            ExpectedEvent(fileName, EventType::DELETED, EventType::RENAMED)};
        REQUIRE(eventWasDetected(watcher, expectedEvents));

        sandbox.createFile(relWatchedDir / fileName);

        expectedEvents = {
            ExpectedEvent(fileName, EventType::CREATED, EventType::RENAMED)};
        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("file renaming")
    {
        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / fileName);

        fs::path newFileName = "renamed_file";

        auto watcher = startWatching();
        sandbox.rename(relWatchedDir / fileName, relWatchedDir / newFileName);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::DELETED | EventType::RENAMED,
                          EventType::MODIFIED),
            ExpectedEvent(newFileName, EventType::CREATED | EventType::RENAMED,
                          EventType::MODIFIED | EventType::DELETED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("file creation, modification, rename, deletion and creation")
    {
        auto watcher = startWatching(40ms);

        fs::path fileName    = "created_file";
        fs::path newFileName = "renamed_file";

        sandbox.createFile(relWatchedDir / fileName);
        sandbox.modifyFile(relWatchedDir / fileName, "content");
        sandbox.rename(relWatchedDir / fileName, relWatchedDir / newFileName);
        sandbox.remove(relWatchedDir / newFileName);
        sandbox.createFile(relWatchedDir / fileName);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::CREATED | EventType::MODIFIED |
                                        EventType::RENAMED),
            ExpectedEvent(newFileName, EventType::RENAMED | EventType::DELETED,
                          EventType::MODIFIED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("move file inside of watched area")
    {
        fs::path subDirectory = "subDirectory";
        sandbox.createDirectory(relWatchedDir / subDirectory);

        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / fileName);

        auto watcher = startWatching();
        sandbox.rename(relWatchedDir / fileName,
                       relWatchedDir / subDirectory / fileName);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::DELETED, EventType::MODIFIED),
            ExpectedEvent(subDirectory / fileName, EventType::CREATED,
                          EventType::MODIFIED | EventType::DELETED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("move file into watched area")
    {
        fs::path fileName    = "created_file";
        fs::path absFilePath = sandbox.createFile(fileName, "content");

        auto watcher = startWatching();
        sandbox.rename(fileName, relWatchedDir / fileName);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::CREATED,
                          EventType::MODIFIED | EventType::DELETED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("move file out of watched area")
    {
        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / fileName);

        auto watcher = startWatching();
        sandbox.rename(relWatchedDir / fileName, fileName);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::DELETED, EventType::MODIFIED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("create file in new created directory")
    {
        auto watcher = startWatching();

        fs::path subDirectory = "subDirectory";
        sandbox.createDirectory(relWatchedDir / subDirectory);

        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / subDirectory / fileName);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(subDirectory, EventType::CREATED,
                          EventType::DELETED | EventType::RENAMED),
            ExpectedEvent(subDirectory / fileName, EventType::CREATED,
                          EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("modify file in new created directory")
    {
        auto watcher = startWatching();

        fs::path subDirectory = "subDirectory";
        sandbox.createDirectory(relWatchedDir / subDirectory);

        fs::path fileName = "created_file";
        sandbox.createFile(relWatchedDir / subDirectory / fileName, "content");

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(subDirectory, EventType::CREATED,
                          EventType::DELETED | EventType::RENAMED),
            ExpectedEvent(subDirectory / fileName, EventType::CREATED,
                          EventType::DELETED | EventType::RENAMED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("modify and rename file")
    {
        fs::path fileName    = "created_file";
        fs::path newFileName = "renamed_file";
        sandbox.createFile(relWatchedDir / fileName);

        auto watcher = startWatching(40ms);
        sandbox.modifyFile(relWatchedDir / fileName, "content");
        sandbox.rename(relWatchedDir / fileName, relWatchedDir / newFileName);

        std::vector<ExpectedEvent> expectedEvents = {
            ExpectedEvent(fileName, EventType::DELETED | EventType::MODIFIED |
                                        EventType::RENAMED),
            ExpectedEvent(newFileName, EventType::CREATED | EventType::RENAMED,
                          EventType::MODIFIED | EventType::DELETED)};

        REQUIRE(eventWasDetected(watcher, expectedEvents));
        CHECK(watcher->isWatching());
    }

    SECTION("Directory")
    {
        SECTION("directory creation")
        {
            auto watcher = startWatching();

            fs::path dirName = "subfolder";
            sandbox.createDirectory(relWatchedDir / dirName);

            std::vector<ExpectedEvent> expectedEvents = {ExpectedEvent(
                dirName, EventType::CREATED,
                EventType::MODIFIED | EventType::DELETED | EventType::RENAMED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("several directory creation in subtree")
        {
            auto watcher = startWatching(40ms);

            fs::path dirName = "subfolder";
            sandbox.createDirectory(relWatchedDir / dirName);

            fs::path dirName2 = "subfolder2";
            sandbox.createDirectory(relWatchedDir / dirName / dirName2);

            fs::path dirName3 = "subfolder3";
            sandbox.createDirectory(relWatchedDir / dirName / dirName2 /
                                    dirName3);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::CREATED,
                              EventType::DELETED | EventType::RENAMED),
                ExpectedEvent(dirName / dirName2, EventType::CREATED,
                              EventType::DELETED | EventType::RENAMED),
                ExpectedEvent(dirName / dirName2 / dirName3, EventType::CREATED,
                              EventType::MODIFIED | EventType::DELETED |
                                  EventType::RENAMED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("several directory creation in subtree with add file")
        {
            auto watcher = startWatching(40ms);

            fs::path dirName = "subfolder";
            sandbox.createDirectory(relWatchedDir / dirName);

            fs::path dirName2 = "subfolder2";
            sandbox.createDirectory(relWatchedDir / dirName / dirName2);

            fs::path dirName3 = "subfolder3";
            sandbox.createDirectory(relWatchedDir / dirName / dirName2 /
                                    dirName3);

            fs::path fileName = "created_file";
            sandbox.createFile(relWatchedDir / dirName / dirName2 / dirName3 /
                               fileName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::CREATED,
                              EventType::DELETED | EventType::RENAMED),
                ExpectedEvent(dirName / dirName2, EventType::CREATED,
                              EventType::DELETED | EventType::RENAMED),
                ExpectedEvent(dirName / dirName2 / dirName3, EventType::CREATED,
                              EventType::DELETED | EventType::RENAMED),
                ExpectedEvent(dirName / dirName2 / dirName3 / fileName,
                              EventType::CREATED,
                              EventType::DELETED | EventType::RENAMED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("empty directory deletion")
        {
            fs::path dirName = "subfolder";
            sandbox.createDirectory(relWatchedDir / dirName);

            auto watcher = startWatching();
            sandbox.remove(relWatchedDir / dirName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED,
                              EventType::MODIFIED | EventType::RENAMED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("directory deletion with file as children")
        {
            fs::path dirName  = "subfolder";
            fs::path fileName = "created_file";
            sandbox.createDirectory(relWatchedDir / dirName);
            sandbox.createFile(relWatchedDir / dirName / fileName);

            auto watcher = startWatching();
            sandbox.remove(relWatchedDir / dirName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName / fileName, EventType::DELETED,
                              EventType::MODIFIED | EventType::RENAMED),
                ExpectedEvent(dirName, EventType::DELETED, EventType::RENAMED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("empty directory rename")
        {
            fs::path dirName    = "subfolder";
            fs::path newDirName = "otherFolder";
            sandbox.createDirectory(relWatchedDir / dirName);

            auto watcher = startWatching();
            sandbox.rename(relWatchedDir / dirName, relWatchedDir / newDirName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED | EventType::RENAMED,
                              EventType::MODIFIED),
                ExpectedEvent(newDirName,
                              EventType::CREATED | EventType::RENAMED,
                              EventType::MODIFIED | EventType::DELETED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("directory rename with file as children")
        {
            fs::path dirName    = "subfolder";
            fs::path newDirName = "otherFolder";
            fs::path fileName   = "created_file";
            sandbox.createDirectory(relWatchedDir / dirName);
            sandbox.createFile(relWatchedDir / dirName / fileName);

            auto watcher = startWatching();
            sandbox.rename(relWatchedDir / dirName, relWatchedDir / newDirName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED | EventType::RENAMED,
                              EventType::MODIFIED),
                ExpectedEvent(newDirName,
                              EventType::CREATED | EventType::RENAMED,
                              EventType::DELETED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents,
                                     {newDirName / fileName}));
            CHECK(watcher->isWatching());
        }

        SECTION("empty directory rename and add file")
        {
            fs::path dirName    = "subfolder";
            fs::path newDirName = "otherFolder";
            sandbox.createDirectory(relWatchedDir / dirName);

            auto watcher = startWatching(40ms);
            sandbox.rename(relWatchedDir / dirName, relWatchedDir / newDirName);

            fs::path fileName = "created_file";
            sandbox.createFile(relWatchedDir / newDirName / fileName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED | EventType::RENAMED,
                              EventType::MODIFIED),
                ExpectedEvent(newDirName,
                              EventType::CREATED | EventType::RENAMED,
                              EventType::DELETED),
                ExpectedEvent(newDirName / fileName, EventType::CREATED,
                              EventType::MODIFIED | EventType::DELETED |
                                  EventType::RENAMED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("empty directory rename and add and modify file")
        {
            fs::path dirName    = "subfolder";
            fs::path newDirName = "otherFolder";
            sandbox.createDirectory(relWatchedDir / dirName);

            auto watcher = startWatching(40ms);
            sandbox.rename(relWatchedDir / dirName, relWatchedDir / newDirName);

            fs::path fileName = "created_file";
            sandbox.createFile(relWatchedDir / newDirName / fileName,
                               "content");

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED | EventType::RENAMED,
                              EventType::MODIFIED),
                ExpectedEvent(newDirName,
                              EventType::CREATED | EventType::RENAMED,
                              EventType::DELETED),
                ExpectedEvent(newDirName / fileName,
                              EventType::CREATED | EventType::MODIFIED,
                              EventType::DELETED | EventType::RENAMED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("move empty directory inside of watched area")
        {
            fs::path newPlace = "newPlace";
            fs::path dirName  = "subFolder";

            sandbox.createDirectory(relWatchedDir / newPlace);
            sandbox.createDirectory(relWatchedDir / dirName);

            auto watcher = startWatching();
            sandbox.rename(relWatchedDir / dirName,
                           relWatchedDir / newPlace / dirName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED, EventType::MODIFIED),
                ExpectedEvent(newPlace / dirName, EventType::CREATED,
                              EventType::DELETED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("move directory with child inside of watched area")
        {
            fs::path newPlace = "newPlace";
            fs::path dirName  = "subFolder";
            fs::path fileName = "created_file";

            sandbox.createDirectory(relWatchedDir / newPlace);
            sandbox.createDirectory(relWatchedDir / dirName);
            sandbox.createFile(relWatchedDir / dirName / fileName);

            auto watcher = startWatching();
            sandbox.rename(relWatchedDir / dirName,
                           relWatchedDir / newPlace / dirName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED, EventType::MODIFIED),
                ExpectedEvent(newPlace / dirName, EventType::CREATED,
                              EventType::DELETED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents,
                                     {newPlace / dirName / fileName}));
            CHECK(watcher->isWatching());
        }

        SECTION("empty directory move and add file")
        {
            fs::path newPlace = "newPlace";
            fs::path dirName  = "subfolder";
            sandbox.createDirectory(relWatchedDir / dirName);
            sandbox.createDirectory(relWatchedDir / newPlace);

            auto watcher = startWatching(40ms);
            sandbox.rename(relWatchedDir / dirName,
                           relWatchedDir / newPlace / dirName);

            fs::path fileName = "created_file";
            sandbox.createFile(relWatchedDir / newPlace / dirName / fileName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED, EventType::MODIFIED),
                ExpectedEvent(newPlace / dirName, EventType::CREATED,
                              EventType::DELETED),
                ExpectedEvent(newPlace / dirName / fileName, EventType::CREATED,
                              EventType::MODIFIED | EventType::DELETED |
                                  EventType::RENAMED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("empty directory move and add and modify file")
        {
            fs::path newPlace = "newPlace";
            fs::path dirName  = "subfolder";
            sandbox.createDirectory(relWatchedDir / dirName);
            sandbox.createDirectory(relWatchedDir / newPlace);

            auto watcher = startWatching(50ms);
            sandbox.rename(relWatchedDir / dirName,
                           relWatchedDir / newPlace / dirName);

            fs::path fileName = "created_file";
            sandbox.createFile(relWatchedDir / newPlace / dirName / fileName,
                               "content");

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED, EventType::MODIFIED),
                ExpectedEvent(newPlace / dirName, EventType::CREATED,
                              EventType::DELETED),
                ExpectedEvent(newPlace / dirName / fileName,
                              EventType::CREATED | EventType::MODIFIED,
                              EventType::DELETED | EventType::RENAMED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("move empty directory into watched area")
        {
            fs::path dirName = "subfolder";
            sandbox.createDirectory(dirName);

            auto watcher = startWatching();
            sandbox.rename(dirName, relWatchedDir / dirName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::CREATED,
                              EventType::MODIFIED | EventType::DELETED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("move directory with entries into watched subfolder")
        {
            fs::path dirName  = "subfolder";
            fs::path fileName = "created_file";
            sandbox.createDirectory(dirName);
            sandbox.createFile(dirName / fileName);

            auto watcher = startWatching();
            sandbox.rename(dirName, relWatchedDir / dirName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::CREATED, EventType::DELETED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents,
                                     {dirName / fileName}));
            CHECK(watcher->isWatching());
        }

        SECTION("move empty directory out of watched area")
        {
            fs::path dirName = "subfolder";
            sandbox.createDirectory(relWatchedDir / dirName);

            auto     watcher = startWatching();
            fs::path newAbsFilePath =
                sandbox.rename(relWatchedDir / dirName, dirName);

            std::vector<ExpectedEvent> expectedEvents = {ExpectedEvent(
                dirName, EventType::DELETED, EventType::MODIFIED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }

        SECTION("move directory with entries out of watched area")
        {
            fs::path dirName  = "subfolder";
            fs::path fileName = "created_file";
            sandbox.createDirectory(relWatchedDir / dirName);
            sandbox.createFile(relWatchedDir / dirName / fileName);

            auto     watcher = startWatching();
            fs::path newAbsFilePath =
                sandbox.rename(relWatchedDir / dirName, dirName);

            std::vector<ExpectedEvent> expectedEvents = {
                ExpectedEvent(dirName, EventType::DELETED)};

            REQUIRE(eventWasDetected(watcher, expectedEvents));
            CHECK(watcher->isWatching());
        }
    }
}
