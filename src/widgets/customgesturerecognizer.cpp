#include "customgesturerecognizer.h"

#include <QDebug>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QWidget>

// quick and dirty qt guesture reimplementation

SwipeGestureRecognizer::SwipeGestureRecognizer()
    : horizontalDirection(QSwipeGesture::NoDirection),
      verticalDirection(QSwipeGesture::NoDirection),
      swipeAngle(0),
      state(NoGesture),
      velocityValue(0)
{
}

QGesture *SwipeGestureRecognizer::create(QObject *)
{
    //    if (target && target->isWidgetType()) {
    //        static_cast<QWidget
    //        *>(target)->setAttribute(Qt::WA_AcceptTouchEvents);
    //    }
    return new QSwipeGesture;
}

static QPoint getTouchPos(QEvent *event)
{
    const QTouchEvent *te = static_cast<const QTouchEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (te->points().isEmpty())
        return QPoint();
    return te->points().first().globalPosition().toPoint();
#else
    if (te->touchPoints().isEmpty())
        return QPoint();
    return te->touchPoints().first().screenPos().toPoint();
#endif
}

QGestureRecognizer::Result SwipeGestureRecognizer::recognize(QGesture *state,
                                                             QObject *,
                                                             QEvent *event)
{
    QSwipeGesture *q = static_cast<QSwipeGesture *>(state);

    QGestureRecognizer::Result result = QGestureRecognizer::Ignore;

    QPoint currentPos;
    bool isPress = false, isRelease = false, isMove = false;

    switch (event->type())
    {
        case QEvent::MouseButtonPress:
            currentPos = static_cast<const QMouseEvent *>(event)->globalPos();
            isPress = true;
            break;
        case QEvent::MouseButtonRelease:
            isRelease = true;
            break;
        case QEvent::MouseMove:
            currentPos = static_cast<const QMouseEvent *>(event)->globalPos();
            isMove = true;
            break;
        case QEvent::TouchBegin:
            currentPos = getTouchPos(event);
            isPress = true;
            break;
        case QEvent::TouchUpdate:
            currentPos = getTouchPos(event);
            isMove = true;
            break;
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            isRelease = true;
            break;
        default:
            return result;
    }

    if (isPress)
    {
        this->velocityValue = 1;
        this->time.start();
        this->state = State::Started;
        q->setHotSpot(currentPos);
        this->startPosition = currentPos;
        result = QGestureRecognizer::MayBeGesture;
    }
    else if (isRelease)
    {
        if (q->state() != Qt::NoGesture)
            result = QGestureRecognizer::FinishGesture;
        else
            result = QGestureRecognizer::CancelGesture;
    }
    else if (isMove)
    {
        if (this->state == State::NoGesture)
        {
            result = QGestureRecognizer::CancelGesture;
        }
        else
        {
            int xDistance = currentPos.x() - this->startPosition.x();
            int yDistance = currentPos.y() - this->startPosition.y();

            const int distance =
                xDistance >= yDistance ? xDistance : yDistance;
            int elapsedTime = this->time.restart();
            if (!elapsedTime)
                elapsedTime = 1;
            this->velocityValue =
                0.9 * this->velocityValue + (qreal)distance / elapsedTime;
            this->swipeAngle =
                QLineF(this->startPosition, currentPos).angle();

            if (qAbs(xDistance) > MoveThreshold ||
                qAbs(yDistance) > MoveThreshold)
            {
                this->startPosition = currentPos;
                q->setSwipeAngle(this->swipeAngle);

                result = QGestureRecognizer::TriggerGesture;
                // QTBUG-46195, small changes in direction should not cause
                // the gesture to be canceled.
                if (this->verticalDirection == QSwipeGesture::NoDirection ||
                    qAbs(yDistance) > directionChangeThreshold)
                {
                    const QSwipeGesture::SwipeDirection vertical =
                        yDistance > 0 ? QSwipeGesture::Down
                                      : QSwipeGesture::Up;
                    if (this->verticalDirection !=
                            QSwipeGesture::NoDirection &&
                        this->verticalDirection != vertical)
                        result = QGestureRecognizer::CancelGesture;
                    this->verticalDirection = vertical;
                }
                if (this->horizontalDirection ==
                        QSwipeGesture::NoDirection ||
                    qAbs(xDistance) > directionChangeThreshold)
                {
                    const QSwipeGesture::SwipeDirection horizontal =
                        xDistance > 0 ? QSwipeGesture::Right
                                      : QSwipeGesture::Left;
                    if (this->horizontalDirection !=
                            QSwipeGesture::NoDirection &&
                        this->horizontalDirection != horizontal)
                        result = QGestureRecognizer::CancelGesture;
                    this->horizontalDirection = horizontal;
                }
            }
            else
            {
                if (q->state() != Qt::NoGesture)
                    result = QGestureRecognizer::TriggerGesture;
                else
                    result = QGestureRecognizer::MayBeGesture;
            }
        }
    }

    return result;
}

void SwipeGestureRecognizer::reset(QGesture *state)
{
    this->verticalDirection = this->horizontalDirection =
        QSwipeGesture::NoDirection;
    this->swipeAngle = 0;

    this->startPosition = QPoint();
    this->state = State::NoGesture;
    this->velocityValue = 0;
    this->time.invalidate();

    QGestureRecognizer::reset(state);
}

TapGestureRecognizer::TapGestureRecognizer() {}

QGesture *TapGestureRecognizer::create(QObject *)
{
    //    if (target && target->isWidgetType()) {
    //        static_cast<QWidget
    //        *>(target)->setAttribute(Qt::WA_AcceptTouchEvents);
    //    }
    return new QTapGesture;
}

QGestureRecognizer::Result TapGestureRecognizer::recognize(QGesture *state,
                                                           QObject *,
                                                           QEvent *event)
{
    QTapGesture *q = static_cast<QTapGesture *>(state);

    QGestureRecognizer::Result result = QGestureRecognizer::CancelGesture;

    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        {
            const QMouseEvent *ev = static_cast<const QMouseEvent *>(event);
            this->position = ev->globalPos();
            q->setHotSpot(this->position);
            timer.start();
            this->pressed = true;
            result = QGestureRecognizer::MayBeGesture;
            break;
        }
        case QEvent::TouchBegin:
        {
            this->position = getTouchPos(event);
            q->setHotSpot(this->position);
            timer.start();
            this->pressed = true;
            result = QGestureRecognizer::MayBeGesture;
            break;
        }
        case QEvent::MouseMove:
        case QEvent::TouchUpdate:
            result = QGestureRecognizer::Ignore;
            break;
        case QEvent::MouseButtonRelease:
        {
            if (q->state() == Qt::NoGesture && this->pressed)
            {
                this->pressed = false;
                const QMouseEvent *ev = static_cast<const QMouseEvent *>(event);
                QPoint delta = ev->globalPos() - this->position.toPoint();
                if (delta.manhattanLength() <= TAPRADIUS &&
                    timer.elapsed() < TIMEOUT)
                    result = QGestureRecognizer::FinishGesture;
                else
                    result = QGestureRecognizer::CancelGesture;
            }
            break;
        }
        case QEvent::TouchEnd:
        {
            if (q->state() == Qt::NoGesture && this->pressed)
            {
                this->pressed = false;
                QPoint p = getTouchPos(event);
                QPoint delta = p - this->position.toPoint();
                if (delta.manhattanLength() <= TAPRADIUS &&
                    timer.elapsed() < TIMEOUT)
                    result = QGestureRecognizer::FinishGesture;
                else
                    result = QGestureRecognizer::CancelGesture;
            }
            break;
        }
        case QEvent::TouchCancel:
            result = QGestureRecognizer::CancelGesture;
            break;
        default:
            result = QGestureRecognizer::Ignore;
            break;
    }
    return result;
}

void TapGestureRecognizer::reset(QGesture *state)
{
    this->pressed = false;
    this->position = QPointF();

    QGestureRecognizer::reset(state);
}
