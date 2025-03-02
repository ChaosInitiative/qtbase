// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qprogressdialog.h"

#if QT_CONFIG(shortcut)
#  include "qshortcut.h"
#endif
#include "qpainter.h"
#include "qdrawutil.h"
#include "qlabel.h"
#include "qprogressbar.h"
#include "qapplication.h"
#include "qstyle.h"
#include "qpushbutton.h"
#include "qtimer.h"
#include "qelapsedtimer.h"
#include "qscopedvaluerollback.h"
#include <private/qdialog_p.h>

#include <QtCore/qpointer.h>

#include <limits.h>

using namespace std::chrono_literals;

QT_BEGIN_NAMESPACE

// If the operation is expected to take this long (as predicted by
// progress time), show the progress dialog.
static constexpr auto defaultShowTime = 4000ms;
// Wait at least this long before attempting to make a prediction.
static constexpr auto minWaitTime = 50ms;

class QProgressDialogPrivate : public QDialogPrivate
{
    Q_DECLARE_PUBLIC(QProgressDialog)

public:
    QProgressDialogPrivate() = default;

    void init(const QString &labelText, const QString &cancelText, int min, int max);
    void layout();
    void retranslateStrings();
    void setCancelButtonText(const QString &cancelButtonText);
    void adoptChildWidget(QWidget *c);
    void ensureSizeIsAtLeastSizeHint();
    void _q_disconnectOnClose();

    QLabel *label = nullptr;
    QPushButton *cancel = nullptr;
    QProgressBar *bar = nullptr;
    QTimer *forceTimer = nullptr;
#ifndef QT_NO_SHORTCUT
    QShortcut *escapeShortcut = nullptr;
#endif
    QPointer<QObject> receiverToDisconnectOnClose;
    QElapsedTimer starttime;
    QByteArray memberToDisconnectOnClose;
    std::chrono::milliseconds showTime = defaultShowTime;
    bool processingEvents = false;
    bool shownOnce = false;
    bool autoClose = true;
    bool autoReset = true;
    bool forceHide = false;
    bool cancellationFlag = false;
    bool setValueCalled = false;
    bool useDefaultCancelText = false;
};

void QProgressDialogPrivate::init(const QString &labelText, const QString &cancelText,
                                  int min, int max)
{
    Q_Q(QProgressDialog);
    label = new QLabel(labelText, q);
    bar = new QProgressBar(q);
    bar->setRange(min, max);
    int align = q->style()->styleHint(QStyle::SH_ProgressDialog_TextLabelAlignment, nullptr, q);
    label->setAlignment(Qt::Alignment(align));
    QObject::connect(q, SIGNAL(canceled()), q, SLOT(cancel()));
    forceTimer = new QTimer(q);
    QObject::connect(forceTimer, SIGNAL(timeout()), q, SLOT(forceShow()));
    if (useDefaultCancelText) {
        retranslateStrings();
    } else {
        q->setCancelButtonText(cancelText);
    }
    starttime.start();
    forceTimer->start(showTime);
}

void QProgressDialogPrivate::layout()
{
    Q_Q(QProgressDialog);
    int sp = q->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, q);
    int mb = q->style()->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, q);
    int ml = qMin(q->width() / 10, q->style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, q));
    int mr = qMin(q->width() / 10, q->style()->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, q));
    const bool centered =
        bool(q->style()->styleHint(QStyle::SH_ProgressDialog_CenterCancelButton, nullptr, q));

    int additionalSpacing = 0;
    QSize cs = cancel ? cancel->sizeHint() : QSize(0,0);
    QSize bh = bar->sizeHint();
    int cspc;
    int lh = 0;

    // Find spacing and sizes that fit.  It is important that a progress
    // dialog can be made very small if the user demands it so.
    for (int attempt=5; attempt--;) {
        cspc = cancel ? cs.height() + sp : 0;
        lh = qMax(0, q->height() - mb - bh.height() - sp - cspc);

        if (lh < q->height()/4) {
            // Getting cramped
            sp /= 2;
            mb /= 2;
            if (cancel) {
                cs.setHeight(qMax(4,cs.height()-sp-2));
            }
            bh.setHeight(qMax(4,bh.height()-sp-1));
        } else {
            break;
        }
    }

    if (cancel) {
        cancel->setGeometry(
            centered ? q->width()/2 - cs.width()/2 : q->width() - mr - cs.width(),
            q->height() - mb - cs.height(),
            cs.width(), cs.height());
    }

    if (label)
        label->setGeometry(ml, additionalSpacing, q->width() - ml - mr, lh);
    bar->setGeometry(ml, lh + sp + additionalSpacing, q->width() - ml - mr, bh.height());
}

void QProgressDialogPrivate::retranslateStrings()
{
    if (useDefaultCancelText)
        setCancelButtonText(QProgressDialog::tr("Cancel"));
}

void QProgressDialogPrivate::_q_disconnectOnClose()
{
    Q_Q(QProgressDialog);
    if (receiverToDisconnectOnClose) {
        QObject::disconnect(q, SIGNAL(canceled()), receiverToDisconnectOnClose,
                            memberToDisconnectOnClose);
        receiverToDisconnectOnClose = nullptr;
    }
    memberToDisconnectOnClose.clear();
}

/*!
  \class QProgressDialog
  \brief The QProgressDialog class provides feedback on the progress of a slow operation.
  \ingroup standard-dialogs
  \inmodule QtWidgets


  A progress dialog is used to give the user an indication of how long
  an operation is going to take, and to demonstrate that the
  application has not frozen. It can also give the user an opportunity
  to abort the operation.

  A common problem with progress dialogs is that it is difficult to know
  when to use them; operations take different amounts of time on different
  hardware.  QProgressDialog offers a solution to this problem:
  it estimates the time the operation will take (based on time for
  steps), and only shows itself if that estimate is beyond minimumDuration()
  (4 seconds by default).

  Use setMinimum() and setMaximum() or the constructor to set the number of
  "steps" in the operation and call setValue() as the operation
  progresses. The number of steps can be chosen arbitrarily. It can be the
  number of files copied, the number of bytes received, the number of
  iterations through the main loop of your algorithm, or some other
  suitable unit. Progress starts at the value set by setMinimum(),
  and the progress dialog shows that the operation has finished when
  you call setValue() with the value set by setMaximum() as its argument.

  The dialog automatically resets and hides itself at the end of the
  operation.  Use setAutoReset() and setAutoClose() to change this
  behavior. Note that if you set a new maximum (using setMaximum() or
  setRange()) that equals your current value(), the dialog will not
  close regardless.

  There are two ways of using QProgressDialog: modal and modeless.

  Compared to a modeless QProgressDialog, a modal QProgressDialog is simpler
  to use for the programmer. Do the operation in a loop, call \l setValue() at
  intervals, and check for cancellation with wasCanceled(). For example:

  \snippet dialogs/dialogs.cpp 3

  A modeless progress dialog is suitable for operations that take
  place in the background, where the user is able to interact with the
  application. Such operations are typically based on a timer class,
  such as QChronoTimer (or the more low-level QObject::timerEvent()) or
  QSocketNotifier; or performed in a separate thread. A QProgressBar in
  the status bar of your main window is often an alternative to a modeless
  progress dialog.

  You need to have an event loop to be running, connect the
  canceled() signal to a slot that stops the operation, and call \l
  setValue() at intervals. For example:

  \snippet dialogs/dialogs.cpp 4
  \codeline
  \snippet dialogs/dialogs.cpp 5
  \codeline
  \snippet dialogs/dialogs.cpp 6

  In both modes the progress dialog may be customized by
  replacing the child widgets with custom widgets by using setLabel(),
  setBar(), and setCancelButton().
  The functions setLabelText() and setCancelButtonText()
  set the texts shown.

  \image fusion-progressdialog.png A progress dialog shown in the Fusion widget style.

  \sa QDialog, QProgressBar
*/


/*!
  Constructs a progress dialog.

  Default settings:
  \list
  \li The label text is empty.
  \li The cancel button text is (translated) "Cancel".
  \li minimum is 0;
  \li maximum is 100
  \endlist

  The \a parent argument is dialog's parent widget. The widget flags, \a f, are
  passed to the QDialog::QDialog() constructor.

  \sa setLabelText(), setCancelButtonText(), setCancelButton(),
  setMinimum(), setMaximum()
*/

QProgressDialog::QProgressDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(*(new QProgressDialogPrivate), parent, f)
{
    Q_D(QProgressDialog);
    d->useDefaultCancelText = true;
    d->init(QString::fromLatin1(""), QString(), 0, 100);
}

/*!
  Constructs a progress dialog.

   The \a labelText is the text used to remind the user what is progressing.

   The \a cancelButtonText is the text to display on the cancel button.  If
   QString() is passed then no cancel button is shown.

   The \a minimum and \a maximum is the number of steps in the operation for
   which this progress dialog shows progress.  For example, if the
   operation is to examine 50 files, this value minimum value would be 0,
   and the maximum would be 50. Before examining the first file, call
   setValue(0). As each file is processed call setValue(1), setValue(2),
   etc., finally calling setValue(50) after examining the last file.

   The \a parent argument is the dialog's parent widget. The parent, \a parent, and
   widget flags, \a f, are passed to the QDialog::QDialog() constructor.

  \sa setLabelText(), setLabel(), setCancelButtonText(), setCancelButton(),
  setMinimum(), setMaximum()
*/

QProgressDialog::QProgressDialog(const QString &labelText,
                                 const QString &cancelButtonText,
                                 int minimum, int maximum,
                                 QWidget *parent, Qt::WindowFlags f)
    : QDialog(*(new QProgressDialogPrivate), parent, f)
{
    Q_D(QProgressDialog);
    d->init(labelText, cancelButtonText, minimum, maximum);
}


/*!
  Destroys the progress dialog.
*/

QProgressDialog::~QProgressDialog()
{
}

/*!
  \fn void QProgressDialog::canceled()

  This signal is emitted when the cancel button is clicked.
  It is connected to the cancel() slot by default.

  \sa wasCanceled()
*/


/*!
  Sets the label to \a label. The progress dialog resizes to fit. The
  label becomes owned by the progress dialog and will be deleted when
  necessary, so do not pass the address of an object on the stack.

  \sa setLabelText()
*/

void QProgressDialog::setLabel(QLabel *label)
{
    Q_D(QProgressDialog);
    if (label == d->label) {
        if (Q_UNLIKELY(label))
            qWarning("QProgressDialog::setLabel: Attempt to set the same label again");
        return;
    }
    delete d->label;
    d->label = label;
    d->adoptChildWidget(label);
}


/*!
  \property QProgressDialog::labelText
  \brief the label's text

  The default text is an empty string.
*/

QString QProgressDialog::labelText() const
{
    Q_D(const QProgressDialog);
    if (d->label)
        return d->label->text();
    return QString();
}

void QProgressDialog::setLabelText(const QString &text)
{
    Q_D(QProgressDialog);
    if (d->label) {
        d->label->setText(text);
        d->ensureSizeIsAtLeastSizeHint();
    }
}


/*!
  Sets the cancel button to the push button, \a cancelButton. The
  progress dialog takes ownership of this button which will be deleted
  when necessary, so do not pass the address of an object that is on
  the stack, i.e. use new() to create the button.  If \nullptr is passed,
  no cancel button will be shown.

  \sa setCancelButtonText()
*/

void QProgressDialog::setCancelButton(QPushButton *cancelButton)
{
    Q_D(QProgressDialog);
    if (d->cancel == cancelButton) {
        if (Q_UNLIKELY(cancelButton))
            qWarning("QProgressDialog::setCancelButton: Attempt to set the same button again");
        return;
    }
    delete d->cancel;
    d->cancel = cancelButton;
    if (cancelButton) {
        connect(d->cancel, SIGNAL(clicked()), this, SIGNAL(canceled()));
#ifndef QT_NO_SHORTCUT
        d->escapeShortcut = new QShortcut(QKeySequence::Cancel, this, SIGNAL(canceled()));
#endif
    } else {
#ifndef QT_NO_SHORTCUT
        delete d->escapeShortcut;
        d->escapeShortcut = nullptr;
#endif
    }
    d->adoptChildWidget(cancelButton);
}

/*!
  Sets the cancel button's text to \a cancelButtonText.  If the text
  is set to QString() then it will cause the cancel button to be
  hidden and deleted.

  \sa setCancelButton()
*/

void QProgressDialog::setCancelButtonText(const QString &cancelButtonText)
{
    Q_D(QProgressDialog);
    d->useDefaultCancelText = false;
    d->setCancelButtonText(cancelButtonText);
}

void QProgressDialogPrivate::setCancelButtonText(const QString &cancelButtonText)
{
    Q_Q(QProgressDialog);

    if (!cancelButtonText.isNull()) {
        if (cancel) {
            cancel->setText(cancelButtonText);
        } else {
            q->setCancelButton(new QPushButton(cancelButtonText, q));
        }
    } else {
        q->setCancelButton(nullptr);
    }
    ensureSizeIsAtLeastSizeHint();
}


/*!
  Sets the progress bar widget to \a bar. The progress dialog resizes to
  fit. The progress dialog takes ownership of the progress \a bar which
  will be deleted when necessary, so do not use a progress bar
  allocated on the stack.
*/

void QProgressDialog::setBar(QProgressBar *bar)
{
    Q_D(QProgressDialog);
    if (Q_UNLIKELY(!bar)) {
        qWarning("QProgressDialog::setBar: Cannot set a null progress bar");
        return;
    }
#ifndef QT_NO_DEBUG
    if (Q_UNLIKELY(value() > 0))
        qWarning("QProgressDialog::setBar: Cannot set a new progress bar "
                  "while the old one is active");
#endif
    if (Q_UNLIKELY(bar == d->bar)) {
        qWarning("QProgressDialog::setBar: Attempt to set the same progress bar again");
        return;
    }
    delete d->bar;
    d->bar = bar;
    d->adoptChildWidget(bar);
}

void QProgressDialogPrivate::adoptChildWidget(QWidget *c)
{
    Q_Q(QProgressDialog);

    if (c) {
        if (c->parentWidget() == q)
            c->hide(); // until after ensureSizeIsAtLeastSizeHint()
        else
            c->setParent(q, { });
    }
    ensureSizeIsAtLeastSizeHint();
    //The layout should be updated again to prevent layout errors when the new 'widget' is replaced
    layout();
    if (c)
        c->show();
}

void QProgressDialogPrivate::ensureSizeIsAtLeastSizeHint()
{
    Q_Q(QProgressDialog);

    QSize size = q->sizeHint();
    if (q->isVisible())
        size = size.expandedTo(q->size());
    q->resize(size);
}


/*!
  \property QProgressDialog::wasCanceled
  \brief whether the dialog was canceled
*/

bool QProgressDialog::wasCanceled() const
{
    Q_D(const QProgressDialog);
    return d->cancellationFlag;
}


/*!
    \property QProgressDialog::maximum
    \brief the highest value represented by the progress bar

    The default is 100.

    \sa minimum, setRange()
*/

int QProgressDialog::maximum() const
{
    Q_D(const QProgressDialog);
    return d->bar->maximum();
}

void QProgressDialog::setMaximum(int maximum)
{
    Q_D(QProgressDialog);
    d->bar->setMaximum(maximum);
}

/*!
    \property QProgressDialog::minimum
    \brief the lowest value represented by the progress bar

    The default is 0.

    \sa maximum, setRange()
*/

int QProgressDialog::minimum() const
{
    Q_D(const QProgressDialog);
    return d->bar->minimum();
}

void QProgressDialog::setMinimum(int minimum)
{
    Q_D(QProgressDialog);
    d->bar->setMinimum(minimum);
}

/*!
    Sets the progress dialog's minimum and maximum values
    to \a minimum and \a maximum, respectively.

    If \a maximum is smaller than \a minimum, \a minimum becomes the only
    legal value.

    If the current value falls outside the new range, the progress
    dialog is reset with reset().

    \sa minimum, maximum
*/
void QProgressDialog::setRange(int minimum, int maximum)
{
    Q_D(QProgressDialog);
    d->bar->setRange(minimum, maximum);
}


/*!
  Resets the progress dialog.
  The progress dialog becomes hidden if autoClose() is true.

  \sa setAutoClose(), setAutoReset()
*/

void QProgressDialog::reset()
{
    Q_D(QProgressDialog);
    if (d->autoClose || d->forceHide)
        hide();
    d->bar->reset();
    d->cancellationFlag = false;
    d->shownOnce = false;
    d->setValueCalled = false;
    d->forceTimer->stop();

    /*
        I wish we could disconnect the user slot provided to open() here but
        unfortunately reset() is usually called before the slot has been invoked.
        (reset() is itself invoked when canceled() is emitted.)
    */
    if (d->receiverToDisconnectOnClose)
        QMetaObject::invokeMethod(this, "_q_disconnectOnClose", Qt::QueuedConnection);
}

/*!
  Resets the progress dialog. wasCanceled() becomes true until
  the progress dialog is reset.
  The progress dialog becomes hidden.
*/

void QProgressDialog::cancel()
{
    Q_D(QProgressDialog);
    d->forceHide = true;
    reset();
    d->forceHide = false;
    d->cancellationFlag = true;
}


int QProgressDialog::value() const
{
    Q_D(const QProgressDialog);
    return d->bar->value();
}

/*!
  \property QProgressDialog::value
  \brief the current amount of progress made.

  For the progress dialog to work as expected, you should initially set
  this property to QProgressDialog::minimum() and finally set it to
  QProgressDialog::maximum(); you can call setValue() any number of times
  in-between.

  \warning If the progress dialog is modal
    (see QProgressDialog::QProgressDialog()),
    setValue() calls QCoreApplication::processEvents(), so take care that
    this does not cause undesirable re-entrancy in your code. For example,
    don't use a QProgressDialog inside a paintEvent()!

  \sa minimum, maximum
*/
void QProgressDialog::setValue(int progress)
{
    Q_D(QProgressDialog);
    if (d->setValueCalled && progress == d->bar->value())
        return;

    d->bar->setValue(progress);

    if (d->shownOnce) {
        if (isModal() && !d->processingEvents) {
            const QScopedValueRollback guard(d->processingEvents, true);
            QCoreApplication::processEvents();
        }
    } else {
        if ((!d->setValueCalled && progress == 0 /* for compat with Qt < 5.4 */) || progress == minimum()) {
            d->starttime.start();
            d->forceTimer->start(d->showTime);
            d->setValueCalled = true;
            return;
        } else {
            d->setValueCalled = true;
            bool need_show = false;
            using namespace std::chrono;
            nanoseconds elapsed = d->starttime.durationElapsed();
            if (elapsed >= d->showTime) {
                need_show = true;
            } else {
                if (elapsed > minWaitTime) {
                    const int totalSteps = maximum() - minimum();
                    const int myprogress = std::max(progress - minimum(), 1);
                    const int remainingSteps = totalSteps - myprogress;
                    nanoseconds estimate;
                    if (remainingSteps >= INT_MAX / elapsed.count())
                        estimate = (remainingSteps / myprogress) * elapsed;
                    else
                        estimate = (elapsed * remainingSteps) / myprogress;
                    need_show = estimate >= d->showTime;
                }
            }
            if (need_show) {
                d->ensureSizeIsAtLeastSizeHint();
                show();
                d->shownOnce = true;
            }
        }
    }

    if (progress == d->bar->maximum() && d->autoReset)
        reset();
}

/*!
  Returns a size that fits the contents of the progress dialog.
  The progress dialog resizes itself as required, so you should not
  need to call this yourself.
*/

QSize QProgressDialog::sizeHint() const
{
    Q_D(const QProgressDialog);
    QSize labelSize = d->label ? d->label->sizeHint() : QSize(0, 0);
    QSize barSize = d->bar->sizeHint();
    int marginBottom = style()->pixelMetric(QStyle::PM_LayoutBottomMargin, 0, this);
    int spacing = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, 0, this);
    int marginLeft = style()->pixelMetric(QStyle::PM_LayoutLeftMargin, 0, this);
    int marginRight = style()->pixelMetric(QStyle::PM_LayoutRightMargin, 0, this);

    int height = marginBottom * 2 + barSize.height() + labelSize.height() + spacing;
    if (d->cancel)
        height += d->cancel->sizeHint().height() + spacing;
    return QSize(qMax(200, labelSize.width() + marginLeft + marginRight), height);
}

/*!\reimp
*/
void QProgressDialog::resizeEvent(QResizeEvent *)
{
    Q_D(QProgressDialog);
    d->layout();
}

/*!
  \reimp
*/
void QProgressDialog::changeEvent(QEvent *ev)
{
    Q_D(QProgressDialog);
    if (ev->type() == QEvent::StyleChange) {
        d->layout();
    } else if (ev->type() == QEvent::LanguageChange) {
        d->retranslateStrings();
    }
    QDialog::changeEvent(ev);
}

/*!
    \property QProgressDialog::minimumDuration
    \brief the time that must pass before the dialog appears

    If the expected duration of the task is less than the
    minimumDuration, the dialog will not appear at all. This prevents
    the dialog popping up for tasks that are quickly over. For tasks
    that are expected to exceed the minimumDuration, the dialog will
    pop up after the minimumDuration time or as soon as any progress
    is set.

    If set to 0, the dialog is always shown as soon as any progress is
    set. The default is 4000 milliseconds.
*/
void QProgressDialog::setMinimumDuration(int ms)
{
    Q_D(QProgressDialog);
    std::chrono::milliseconds msecs{ms};
    d->showTime = msecs;
    if (d->bar->value() == d->bar->minimum()) {
        d->forceTimer->stop();
        d->forceTimer->start(msecs);
    }
}

int QProgressDialog::minimumDuration() const
{
    Q_D(const QProgressDialog);
    return int(d->showTime.count());
}


/*!
  \reimp
*/

void QProgressDialog::closeEvent(QCloseEvent *e)
{
    emit canceled();
    QDialog::closeEvent(e);
}

/*!
  \property QProgressDialog::autoReset
  \brief whether the progress dialog calls reset() as soon as value() equals maximum().

  The default is true.

  \sa setAutoClose()
*/

void QProgressDialog::setAutoReset(bool b)
{
    Q_D(QProgressDialog);
    d->autoReset = b;
}

bool QProgressDialog::autoReset() const
{
    Q_D(const QProgressDialog);
    return d->autoReset;
}

/*!
  \property QProgressDialog::autoClose
  \brief whether the dialog gets hidden by reset()

  The default is true.

  \sa setAutoReset()
*/

void QProgressDialog::setAutoClose(bool close)
{
    Q_D(QProgressDialog);
    d->autoClose = close;
}

bool QProgressDialog::autoClose() const
{
    Q_D(const QProgressDialog);
    return d->autoClose;
}

/*!
  \reimp
*/

void QProgressDialog::showEvent(QShowEvent *e)
{
    Q_D(QProgressDialog);
    QDialog::showEvent(e);
    d->ensureSizeIsAtLeastSizeHint();
    d->forceTimer->stop();
}

/*!
  Shows the dialog if it is still hidden after the algorithm has been started
  and minimumDuration milliseconds have passed.

  \sa setMinimumDuration()
*/

void QProgressDialog::forceShow()
{
    Q_D(QProgressDialog);
    d->forceTimer->stop();
    if (d->shownOnce || d->cancellationFlag)
        return;

    show();
    d->shownOnce = true;
}

/*!
    \since 4.5

    Opens the dialog and connects its canceled() signal to the slot specified
    by \a receiver and \a member.

    The signal will be disconnected from the slot when the dialog is closed.
*/
void QProgressDialog::open(QObject *receiver, const char *member)
{
    Q_D(QProgressDialog);
    connect(this, SIGNAL(canceled()), receiver, member);
    d->receiverToDisconnectOnClose = receiver;
    d->memberToDisconnectOnClose = member;
    QDialog::open();
}

QT_END_NAMESPACE

#include "moc_qprogressdialog.cpp"
