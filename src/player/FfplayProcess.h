#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

class QProcess;
class QTimer;

/**
 * Platform helpers around native windows. Only Windows is implemented;
 * everywhere else these are no-ops so the application still builds.
 */
namespace NativeWindow {
quintptr findProcessWindow(qint64 pid, quintptr hostHint = 0);
bool sendKey(quintptr handle, int virtualKey);
quintptr createChildHost(quintptr parent, int x, int y, int width, int height);
void setChildGeometry(quintptr child, int x, int y, int width, int height);
void setChildVisible(quintptr child, bool visible);
void attachToHost(quintptr child, quintptr host);
void destroyWindow(quintptr handle);
} // namespace NativeWindow

/**
 * Thin wrapper around an ffplay process.
 *
 * Besides launching the process it owns the little state model ffplay forces
 * on us: keyboard driven volume/mute/seek, the stderr status line used for
 * progress and the process' native window handle.
 */
class FfplayProcess : public QObject
{
    Q_OBJECT

public:
    explicit FfplayProcess(QObject *parent = nullptr);
    ~FfplayProcess() override;

    void setProgram(const QString &program);
    QString program() const { return m_program; }

    void setSeekInterval(double seconds);
    double seekInterval() const { return m_seekInterval; }

    /** Optional container window used when looking for the ffplay window. */
    void setWindowHostHint(quintptr host);

    /**
     * Starts ffplay. `windowArgs` are the window specific options
     * (`-noborder -x -y` for the embedded backend).
     */
    void start(const QString &filePath,
               double startSeconds,
               const QStringList &windowArgs,
               const QString &windowTitle,
               const QProcessEnvironment &environment);

    void stop();

    bool isRunning() const;
    qint64 processId() const { return m_pid; }

    /** Sends a single ffplay keyboard shortcut (e.g. 'p', 'q', 'm'). */
    void sendShortcut(char key);

    /** Volume is expressed on the same 0-100 scale as the -volume option. */
    int volume() const { return m_volume; }
    void requestVolume(int target);

    bool muted() const { return m_muted; }
    void requestMute(bool muted);

    /** Presses arrow keys to move `delta` seconds; false when a restart is needed. */
    bool requestRelativeSeek(double delta);

    quintptr windowHandle() const { return m_window; }

signals:
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void finished(int exitCode);
    void failed(const QString &message);
    void windowFound(quintptr handle);

private:
    void handleStandardError();
    void pollForWindow();
    void resetWindowTracking();

    QProcess *m_process = nullptr;
    QTimer *m_windowTimer = nullptr;
    QString m_program;
    QString m_stderrBuffer;
    QString m_filePath;
    double m_seekInterval = 10.0;
    int m_volume = 100;
    bool m_muted = false;
    qint64 m_pid = 0;
    quintptr m_window = 0;
    int m_windowPolls = 0;
    quintptr m_windowHostHint = 0;
};
