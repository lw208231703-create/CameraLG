#ifndef NOISE_RESULT_MODEL_H
#define NOISE_RESULT_MODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QVariant>
#include <QModelIndex>

class NoiseResultModel : public QAbstractTableModel
{
public:
    explicit NoiseResultModel(const QVector<double>& data, int width, int height, int startX, int startY, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    QVector<double> m_data;
    int m_width;
    int m_height;
    int m_startX;
    int m_startY;
};

#endif // NOISE_RESULT_MODEL_H
