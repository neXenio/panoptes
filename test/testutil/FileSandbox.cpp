/*#include "FileSandbox.h"
#include "testutil/FileSandbox.h"

#include "catch_wrapper.h"

#include <QDataStream>
#include <QDebug>
#include <QDirIterator>
#include <QTest>

#include "dvault/internal/definitions.h"
#include "dvault/util/Platform.h"

#include "testutil/stout.h"
#include "testutil/util.h"

using namespace dvault;
using namespace fakeit;

void FileSandbox::createFile(const RelativePath &relativePath)
{
    createFile(relativePath, QByteArray());
}

void FileSandbox::createFile(const RelativePath &relativePath,
                             const QByteArray &  content)
{
    QFile file(path() / relativePath);
    file.open(QIODevice::ReadWrite);
    file.write(content);
    file.close();
    REQUIRE(file.exists());
}

bool FileSandbox::createSymLink(RelativePath srcPath, RelativePath dstPath)
{
    if (!(path() / dstPath).exists()) {
        return false;
    }

#ifdef DVAULT_WINDOWS
    srcPath = RelativePath(srcPath.toString() + ".lnk");
#endif

    return QFile((path() / dstPath)).link((path() / srcPath));
}

void FileSandbox::createRandomFile(const RelativePath &relativePath,
                                   const unsigned int  fileSize)
{
    createFile(relativePath, getRandomData(fileSize));
}

void FileSandbox::modifyFile(const RelativePath &relativePath,
                             const QByteArray &  content)
{
    QFile file(path() / relativePath);
    REQUIRE(file.exists());

    file.open(QIODevice::ReadWrite);
    file.write(content);
    file.close();
}

void FileSandbox::modifyFileRandomly(const RelativePath &relativePath,
                                     const unsigned int  fileSize)
{
    modifyFile(relativePath, getRandomData(fileSize));
}

AbsolutePath FileSandbox::createDirectory(const RelativePath &relativePath)
{
    QDir(path()).mkpath(relativePath);
    const auto result = path() / relativePath;
    CHECK(result.exists());
    return result;
}

void FileSandbox::touch(const RelativePath &relativePath)
{
    const auto absolutePath = path() / relativePath;
    const auto currentTime  = FileTime(QDateTime::currentDateTimeUtc());

    //! \note(pbatram): Unfortunately some tests are very flaky if we use the
    //! current time and run tests in parallel. We add one second just to be
    //! sure that modifiedTime changes.
    updateFileSystemModificationTime(absolutePath, currentTime.addSecs(1));
}

void FileSandbox::remove(const RelativePath &relativePath)
{
    const auto absolutePath = path() / relativePath;
    QFileInfo  info(absolutePath);
    if (!info.exists()) {
        return;
    }

    if (info.isDir()) {
        CHECK(QDir(absolutePath).removeRecursively());
    } else {
        CHECK(QDir(absolutePath.parent())
                  .remove(absolutePath.fileName().toQString()));
    }

    waitForRemoval(absolutePath);
}

void FileSandbox::rename(const RelativePath &relativePathOld,
                         const RelativePath &relativePathNew)
{
    const auto absolutePathOld = path() / relativePathOld;
    const auto absolutePathNew = path() / relativePathNew;
    QFileInfo  info(absolutePathOld);
    CHECK(info.exists());
    CHECK(QDir(absolutePathOld.parent())
              .rename(absolutePathOld.fileName().toQString(), absolutePathNew));
}

QByteArray FileSandbox::read(const RelativePath &relativePath)
{
    QFile file(path() / relativePath);
    REQUIRE(file.exists());
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    file.close();
    return data;
}

bool FileSandbox::isEmpty() const
{
    QDirIterator it(path(), QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);
    return !it.hasNext();
}

bool FileSandbox::contains(const RelativePath &relativePath) const
{
    return (path() / relativePath).exists();
}

bool FileSandbox::containsFile(const RelativePath &relativePath) const
{
    return contains(relativePath) && QFileInfo(path() / relativePath).isFile();
}

bool FileSandbox::containsDirectory(const RelativePath &relativePath) const
{
    return contains(relativePath) && QFileInfo(path() / relativePath).isDir();
}

QList<RelativePath> FileSandbox::list() const
{
    QDirIterator itr(path(), QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot,
                     QDirIterator::Subdirectories);
    QList<RelativePath> result;
    while (itr.hasNext()) {
        result.append(AbsolutePath(itr.next()).relativeToBase(path()));
    }
    return result;
}

int FileSandbox::entryCount() const
{
    QDirIterator it(path(), QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files,
                    QDirIterator::Subdirectories);
    int          count = 0;
    for (; it.hasNext(); it.next(), ++count) {
    }
    return count;
}

void FileSandbox::printListing() const
{
    for (auto file : list()) {
        qDebug() << file;
    }
}

void FileSandbox::waitForRemoval(const AbsolutePath &absolutePath,
                                 const int           timeout) const
{
    // Windows seems to delete directory entries asynchronously.
    // Hence, we repeatedly check the removal with a short timeout
    const int qwait_time       = 10;
    const int timeout_attempts = timeout / qwait_time;

    for (int i = 0; i < timeout_attempts; ++i) {
        if (!absolutePath.exists()) {
            return;
        }
        QTest::qWait(qwait_time);
    }

    FAIL(QString("failed to delete '%1' in %2 milliseconds.")
             .arg(absolutePath, QString::number(timeout))
             .toStdString());
}

QString FileSandbox::getFileSystemType() const
{
    Try<QString> res = ::getFileSystemType(path());
    if (res.isError()) {
        FAIL(QString("cannot get type: '%1'")
                 .arg(res.error().c_str())
                 .toStdString());
    }
    return res.get();
}

void FileSandbox::prepareRamlExample()
{
    // see CloudRaid-REST/src/raml/examples/treenodes_children_examples.raml
    createDirectory(RelativePath("funny_kittens"));
    createFile(RelativePath("testfile1.txt"));
    createFile(RelativePath("funny_kittens/kitteh.jpg"));
    createFile(RelativePath("funny_kittens/icanhazcheezeburger.jpg"));
}

void FileSandbox::snapshotJournalManagerMock(
    Mock<sync::JournalManager> &mock) const
{
    When(Method(mock, existingChildren))
        .AlwaysReturn(std::make_tuple(VolumeID::generate(),
                                      std::vector<sync::SyncOperation>{}));

    const auto rootEntry = FileMetaInfo::fromPath(path(), path());
    const auto rootOp    = sync::SyncOperation::createAddOperation(
        rootEntry, DeviceID::generate(), VolumeID::generate(), std::nullopt);

    snapshotJournalManagerRecursively(path(), rootOp, &mock);
}

void FileSandbox::addListingTo(
    Mock<sync::JournalManager> *            mockPtr,
    const std::vector<sync::SyncOperation> &operations,
    const RelativePath &                    relativePath) const
{
    Mock<sync::JournalManager> &mock = *mockPtr;
    When(Method(mock, existingChildren).Using(relativePath))
        .AlwaysReturn(std::make_tuple(VolumeID::generate(), operations));
}*/
