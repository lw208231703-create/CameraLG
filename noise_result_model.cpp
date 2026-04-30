#include "noise_result_model.h"

NoiseResultModel::NoiseResultModel(const QVector<double>& data, int width, int height, int startX, int startY, QObject* parent)
    : QAbstractTableModel(parent)
    , m_data(data)
    , m_width(width)
    , m_height(height)
    , m_startX(startX)
    , m_startY(startY)
{
}

int NoiseResultModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_height;
}

int NoiseResultModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_width;
}

QVariant NoiseResultModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return QVariant();

    if (role == Qt::DisplayRole) {
        int idx = index.row() * m_width + index.column();
        if (idx >= 0 && idx < m_data.size()) {
            return QString::number(m_data[idx], 'f', 2);
        }
    } else if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    return QVariant();
}

QVariant NoiseResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            return QString::number(m_startX + section);
        } else {
            return QString::number(m_startY + section);
        }
    }
    return QVariant();
}
