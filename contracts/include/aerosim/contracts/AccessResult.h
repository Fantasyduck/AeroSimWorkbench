#pragma once

#include <QString>
#include <QtGlobal>

#include <utility>

namespace aerosim {

// AccessStatus describes whether a synchronous lightweight query completed.
// It does not describe whether a business action is allowed; use
// ValidationResult for business validation.
enum class AccessStatus {
    Ok,
    NotFound,
    InvalidArgument,
    Conflict,
    StaleRevision,
    Unavailable,
    Unsupported,
    IoError,
    InternalError
};

template <typename T>
class AccessResult {
public:
    AccessResult() = default;

    static AccessResult success(T value)
    {
        AccessResult result;
        result.status_ = AccessStatus::Ok;
        result.value_ = std::move(value);
        result.hasValue_ = true;
        return result;
    }

    static AccessResult failure(AccessStatus status, const QString &errorCode,
                                const QString &message)
    {
        AccessResult result;
        if (status == AccessStatus::Ok) {
            result.status_ = AccessStatus::InternalError;
            result.errorCode_ = QStringLiteral("contracts.access.invalid_failure_status");
            result.message_ = QStringLiteral("A failed access result cannot use status Ok");
            return result;
        }
        result.status_ = status;
        result.errorCode_ = errorCode;
        result.message_ = message;
        return result;
    }

    bool ok() const { return status_ == AccessStatus::Ok; }
    explicit operator bool() const { return ok(); }
    bool hasValue() const { return hasValue_; }

    AccessStatus status() const { return status_; }
    const QString &errorCode() const { return errorCode_; }
    const QString &message() const { return message_; }
    const T &value() const
    {
        Q_ASSERT(hasValue_);
        return value_;
    }
    T &value()
    {
        Q_ASSERT(hasValue_);
        return value_;
    }

private:
    AccessStatus status_ = AccessStatus::InternalError;
    QString errorCode_;
    QString message_;
    T value_{};
    bool hasValue_ = false;
};

} // namespace aerosim
