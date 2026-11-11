#pragma once

#include "aerosim/ui/intelligent/ResultDataSource.h"

namespace aerosim {

// Turns a monitor history file (CSV or Tecplot ASCII) into a ResultTable.
//
// This is the only place where the pages meet the monitor reader. Keeping the
// conversion here rather than in the reader means the reader keeps exactly the
// interface it already had -- MonitorLog, one vector per column -- while the
// pages get the transposed view they present.
class MonitorResultDataSource final : public ResultDataSource {
public:
    MonitorResultDataSource() = default;

    bool load(const QString &path, ResultTable *table, QString *error) override;
    QString fileFilter() const override;
};

} // namespace aerosim
