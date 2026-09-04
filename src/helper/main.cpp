#include "Flasher.h"

#include <QCommandLineParser>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omsticker-helper"));
    app.setApplicationVersion(QStringLiteral("0.9.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Privileged OmSticker USB writer"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("flash"));
    parser.addOption({QStringLiteral("iso"), QStringLiteral("ISO image path"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("device"), QStringLiteral("Whole-disk block device"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("filesystem"), QStringLiteral("Data partition filesystem"),
                      QStringLiteral("fs"), QStringLiteral("exfat")});
    parser.addOption({QStringLiteral("label"), QStringLiteral("Data volume label"),
                      QStringLiteral("name"), QStringLiteral("OMARCHY")});
    parser.addOption({QStringLiteral("no-data-partition"),
                      QStringLiteral("Do not use leftover space")});
    parser.addOption({QStringLiteral("offset"), QStringLiteral("Resume write at this byte offset"),
                      QStringLiteral("bytes"), QStringLiteral("0")});
    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (positional.size() != 1 || positional.first() != QLatin1String("flash")) {
        parser.showHelp(1);
    }

    Flasher flasher;
    flasher.isoPath = parser.value(QStringLiteral("iso"));
    flasher.devicePath = parser.value(QStringLiteral("device"));
    flasher.filesystem = parser.value(QStringLiteral("filesystem")).toLower();
    flasher.label = parser.value(QStringLiteral("label"));
    flasher.makeDataPartition = !parser.isSet(QStringLiteral("no-data-partition"));
    flasher.resumeOffset = parser.value(QStringLiteral("offset")).toLongLong();
    return flasher.run();
}
