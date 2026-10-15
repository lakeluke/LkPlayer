#include "player/FfplayProcess.h"

#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

Q_LOGGING_CATEGORY(lcFfplay, "lk.player.ffplay")

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr int kWindowPollIntervalMs = 80;
constexpr int kMaxWindowPolls = 60;

#ifdef Q_OS_WIN

struct WindowSearch {
    DWORD pid;
    HWND result;
    LONG minWidth;
};

BOOL CALLBACK enumProcessWindow(HWND hwnd, LPARAM lParam)
{
    auto *search = reinterpret_cast<WindowSearch *>(lParam);

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != search->pid || !IsWindowVisible(hwnd))
        return TRUE;

    RECT rect{};
    if (!GetClientRect(hwnd, &rect))
        return TRUE;
    if ((rect.right - rect.left) < search->minWidth)
        return TRUE;

    search->result = hwnd;
    return FALSE;
}

int virtualKeyForChar(char key)
{
    if (key == ' ')
        return VK_SPACE;
    const char upper = static_cast<char>(::toupper(static_cast<unsigned char>(key)));
    return upper;
}

#endif // Q_OS_WIN

} // namespace

// ---------------------------------------------------------------------------
// NativeWindow
// ---------------------------------------------------------------------------

namespace NativeWindow {

quintptr findProcessWindow(qint64 pid, quintptr hostHint)
{
#ifdef Q_OS_WIN
    if (pid == 0)
        return 0;

    // A window that SDL created inside our own host is unambiguous, so the size
    // filter is relaxed: the child is legitimately tiny until the QML layout has
    // reported a real surface rectangle.
    if (hostHint != 0) {
        WindowSearch childSearch{ static_cast<DWORD>(pid), nullptr, 1 };
        EnumChildWindows(reinterpret_cast<HWND>(hostHint), enumProcessWindow, reinterpret_cast<LPARAM>(&childSearch));
        if (childSearch.result)
            return reinterpret_cast<quintptr>(childSearch.result);
    }

    WindowSearch search{ static_cast<DWORD>(pid), nullptr, 17 };
    EnumWindows(enumProcessWindow, reinterpret_cast<LPARAM>(&search));
    return reinterpret_cast<quintptr>(search.result);
#else
    Q_UNUSED(pid);
    Q_UNUSED(hostHint);
    return 0;
#endif
}

bool sendKey(quintptr handle, int virtualKey)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(handle);
    if (!hwnd || !IsWindow(hwnd))
        return false;

    const UINT scan = MapVirtualKeyW(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC);
    LPARAM lParam = static_cast<LPARAM>(1 | (static_cast<LPARAM>(scan) << 16));
    if (virtualKey >= VK_PRIOR && virtualKey <= VK_DOWN)
        lParam |= (1 << 24); // extended key

    PostMessageW(hwnd, WM_KEYDOWN, static_cast<WPARAM>(virtualKey), lParam);
    PostMessageW(hwnd, WM_KEYUP, static_cast<WPARAM>(virtualKey), lParam | (1 << 30) | (1 << 31));
    return true;
#else
    Q_UNUSED(handle);
    Q_UNUSED(virtualKey);
    return false;
#endif
}

quintptr createChildHost(quintptr parent, int x, int y, int width, int height)
{
#ifdef Q_OS_WIN
    static const wchar_t *kClassName = L"LkPlayerVideoHost";
    static bool registered = false;

    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = instance;
        wc.lpszClassName = kClassName;
        wc.hbrBackground = nullptr;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&wc);
        registered = true;
    }

    HWND host = CreateWindowExW(0,
                                kClassName,
                                L"",
                                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                x,
                                y,
                                qMax(1, width),
                                qMax(1, height),
                                reinterpret_cast<HWND>(parent),
                                nullptr,
                                instance,
                                nullptr);
    return reinterpret_cast<quintptr>(host);
#else
    Q_UNUSED(parent);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);
    return 0;
#endif
}

void setChildGeometry(quintptr child, int x, int y, int width, int height)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(child);
    if (!hwnd || !IsWindow(hwnd))
        return;
    SetWindowPos(hwnd,
                 nullptr,
                 x,
                 y,
                 qMax(1, width),
                 qMax(1, height),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
#else
    Q_UNUSED(child);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);
#endif
}

void setChildVisible(quintptr child, bool visible)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(child);
    if (!hwnd || !IsWindow(hwnd))
        return;
    ShowWindow(hwnd, visible ? SW_SHOWNA : SW_HIDE);
#else
    Q_UNUSED(child);
    Q_UNUSED(visible);
#endif
}

void attachToHost(quintptr child, quintptr host)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(child);
    HWND parent = reinterpret_cast<HWND>(host);
    if (!hwnd || !IsWindow(hwnd) || !parent || !IsWindow(parent))
        return;

    SetWindowLongPtrW(hwnd, GWL_STYLE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, 0);
    SetParent(hwnd, parent);
    ShowWindow(hwnd, SW_SHOWNA);
#else
    Q_UNUSED(child);
    Q_UNUSED(host);
#endif
}

void destroyWindow(quintptr handle)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(handle);
    if (hwnd && IsWindow(hwnd))
        DestroyWindow(hwnd);
#else
    Q_UNUSED(handle);
#endif
}

} // namespace NativeWindow

// ---------------------------------------------------------------------------
// FfplayProcess
// ---------------------------------------------------------------------------

FfplayProcess::FfplayProcess(QObject *parent)
    : QObject(parent)
    , m_windowTimer(new QTimer(this))
{
    m_windowTimer->setInterval(kWindowPollIntervalMs);
    connect(m_windowTimer, &QTimer::timeout, this, &FfplayProcess::pollForWindow);
}

FfplayProcess::~FfplayProcess()
{
    stop();
}

void FfplayProcess::setProgram(const QString &program)
{
    m_program = program;
}

void FfplayProcess::setSeekInterval(double seconds)
{
    m_seekInterval = seconds > 0.0 ? seconds : 10.0;
}

void FfplayProcess::setWindowHostHint(quintptr host)
{
    m_windowHostHint = host;
}

bool FfplayProcess::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void FfplayProcess::resetWindowTracking()
{
    m_window = 0;
    m_pid = 0;
    m_windowPolls = 0;
}

void FfplayProcess::start(const QString &filePath,
                          double startSeconds,
                          const QStringList &windowArgs,
                          const QString &windowTitle,
                          const QProcessEnvironment &environment)
{
    stop();

    m_stderrBuffer.clear();
    m_filePath = filePath;
    m_volume = 100;
    m_muted = false;

    if (m_program.isEmpty() || !QFileInfo::exists(m_program)) {
        emit failed(tr("未找到 ffplay 可执行文件，请在设置中指定程序目录"));
        return;
    }

    QStringList args = windowArgs;
    args << QStringLiteral("-seek_interval") << QString::number(m_seekInterval, 'f', 0)
         << QStringLiteral("-volume") << QString::number(m_volume);
    if (!windowTitle.isEmpty())
        args << QStringLiteral("-window_title") << windowTitle;
    if (startSeconds > 0.5)
        args << QStringLiteral("-ss") << QString::number(startSeconds, 'f', 3);
    args << filePath;

    m_process = new QProcess(this);
    m_process->setProcessEnvironment(environment);
    m_process->setProgram(m_program);
    m_process->setArguments(args);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardError, this, &FfplayProcess::handleStandardError);
    connect(m_process, &QProcess::started, this, [this]() {
        m_pid = m_process ? m_process->processId() : 0;
        m_windowPolls = 0;
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            emit failed(tr("ffplay 启动失败：%1").arg(m_program));
    });
    connect(m_process,
            &QProcess::finished,
            this,
            [this](int exitCode, QProcess::ExitStatus status) {
                m_windowTimer->stop();
                const QString tail = m_stderrBuffer.trimmed();
                m_stderrBuffer.clear();
                resetWindowTracking();

                if (status == QProcess::CrashExit) {
                    emit failed(tr("ffplay 进程异常终止"));
                } else if (exitCode != 0) {
                    QString message = tr("ffplay 退出（代码 %1）").arg(exitCode);
                    if (!tail.isEmpty())
                        message += QStringLiteral("：") + tail.right(300);
                    emit failed(message);
                }
                emit finished(exitCode);
            });

    qCInfo(lcFfplay) << "starting" << m_program << args;
    m_process->start();
    m_windowTimer->start();
}

void FfplayProcess::stop()
{
    m_windowTimer->stop();

    if (!m_process) {
        resetWindowTracking();
        return;
    }

    if (m_process->state() != QProcess::NotRunning) {
        m_process->write("q");
        m_process->waitForBytesWritten(80);
        if (m_window)
            NativeWindow::sendKey(m_window, 'Q');

        if (!m_process->waitForFinished(600)) {
            m_process->terminate();
            if (!m_process->waitForFinished(800)) {
                m_process->kill();
                m_process->waitForFinished(400);
            }
        }
    }

    m_process->disconnect(this);
    m_process->deleteLater();
    m_process = nullptr;
    resetWindowTracking();
}

void FfplayProcess::sendShortcut(char key)
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write(QByteArray(1, key));
        m_process->waitForBytesWritten(20);
    }

    if (m_window) {
        int vk = key;
        if (key == ' ')
            vk = 0x20; // VK_SPACE
        NativeWindow::sendKey(m_window, vk);
    }
}

void FfplayProcess::requestVolume(int target)
{
    target = qBound(0, target, 100);
    if (!isRunning()) {
        m_volume = target;
        return;
    }

    // ffplay mixes on a 0..128 scale that '0'/'9' step by one.
    const int currentSteps = qRound(m_volume * 128.0 / 100.0);
    const int targetSteps = qRound(target * 128.0 / 100.0);
    const int steps = qAbs(targetSteps - currentSteps);
    const char key = targetSteps < currentSteps ? '0' : '9';
    for (int i = 0; i < steps; ++i)
        sendShortcut(key);

    m_volume = target;
}

void FfplayProcess::requestMute(bool muted)
{
    if (muted == m_muted)
        return;
    if (isRunning())
        sendShortcut('m');
    m_muted = muted;
}

bool FfplayProcess::requestRelativeSeek(double delta)
{
    if (!isRunning())
        return false;

    // Without a window handle we cannot deliver keyboard shortcuts reliably,
    // so let the caller fall back to restarting the process.
    if (m_window == 0)
        return false;

    const double step = m_seekInterval > 0.0 ? m_seekInterval : 10.0;
    const int presses = qRound(qAbs(delta) / step);
    if (presses == 0)
        return true;
    if (presses > 30)
        return false;

    const int vk = delta > 0.0 ? 0x27 /*VK_RIGHT*/ : 0x25 /*VK_LEFT*/;
    for (int i = 0; i < presses; ++i)
        NativeWindow::sendKey(m_window, vk);
    return true;
}

void FfplayProcess::pollForWindow()
{
    if (!isRunning()) {
        m_windowTimer->stop();
        return;
    }

    if (m_window == 0) {
        const quintptr found = NativeWindow::findProcessWindow(m_pid, m_windowHostHint);
        if (found != 0) {
            m_window = found;
            m_windowTimer->stop();
            qCInfo(lcFfplay) << "ffplay window found";
            emit windowFound(found);
            return;
        }
    }

    if (++m_windowPolls > kMaxWindowPolls)
        m_windowTimer->stop();
}

void FfplayProcess::handleStandardError()
{
    if (!m_process)
        return;

    m_stderrBuffer += QString::fromLocal8Bit(m_process->readAllStandardError());

    static const QRegularExpression durationRe(
        QStringLiteral("Duration:\\s*(\\d+):(\\d+):(\\d+(?:\\.\\d+)?)"));
    static const QRegularExpression positionRe(QStringLiteral("([0-9]+\\.[0-9]+)\\s+M-[AV]:"));

    int start = 0;
    for (int i = 0; i < m_stderrBuffer.size(); ++i) {
        const QChar ch = m_stderrBuffer.at(i);
        if (ch != QLatin1Char('\r') && ch != QLatin1Char('\n'))
            continue;

        const QString line = m_stderrBuffer.mid(start, i - start).trimmed();
        start = i + 1;
        if (line.isEmpty())
            continue;

        const auto durationMatch = durationRe.match(line);
        if (durationMatch.hasMatch()) {
            const double seconds = durationMatch.captured(1).toInt() * 3600.0
                                   + durationMatch.captured(2).toInt() * 60.0
                                   + durationMatch.captured(3).toDouble();
            if (seconds > 0.0)
                emit durationChanged(seconds);
        }

        const auto positionMatch = positionRe.match(line);
        if (positionMatch.hasMatch())
            emit positionChanged(positionMatch.captured(1).toDouble());
    }

    if (start > 0)
        m_stderrBuffer.remove(0, start);
    if (m_stderrBuffer.size() > 16384)
        m_stderrBuffer = m_stderrBuffer.right(4096);
}
