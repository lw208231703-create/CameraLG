#ifndef IMAGE_PROCESSING_PANEL_H
#define IMAGE_PROCESSING_PANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QMap>
#include <QVariantMap>
#include "image_algorithm_base.h"
#include "image_algorithm_manager.h"

class QSplitter;

/**
 * @brief Panel for algorithm parameter editing
 * 
 * Dynamically generates UI controls based on algorithm parameters
 */
class AlgorithmParameterPanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit AlgorithmParameterPanel(QWidget *parent = nullptr);
    
    /**
     * @brief Set the algorithm to display parameters for
     * @param info Algorithm information
     */
    void setAlgorithm(const AlgorithmInfo &info);

    /**
     * @brief Apply parameters to the current widgets (does not emit parametersChanged).
     */
    void setParameters(const QVariantMap &params);
    
    /**
     * @brief Get current parameter values
     */
    QVariantMap getParameters() const;
    
signals:
    /**
     * @brief Emitted when any parameter value changes
     */
    void parametersChanged(const QVariantMap &params);
    
private slots:
    void onParameterChanged();
    
private:
    void clearParameters();
    QWidget* createParameterWidget(const AlgorithmParameter &param);
    
    AlgorithmInfo m_currentAlgorithm;
    
    QVBoxLayout *m_mainLayout;
    QLabel *m_titleLabel;
    QLabel *m_descriptionLabel;
    QWidget *m_parametersContainer;
    QVBoxLayout *m_parametersLayout;
    
    // Map parameter name to its widget
    QMap<QString, QWidget*> m_parameterWidgets;
};

/**
 * @brief Image Processing panel with tree navigation and parameter editing
 * 
 * Main UI panel for selecting and configuring image processing algorithms
 */
class ImageProcessingPanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit ImageProcessingPanel(QWidget *parent = nullptr);
    ~ImageProcessingPanel();
    
    /**
     * @brief Set the algorithm manager
     * @param manager Algorithm manager instance
     */
    void setAlgorithmManager(ImageAlgorithmManager *manager);

signals:
    /**
     * @brief Diagnostic message emitted by the panel (for UI logging)
     */
    void diagnosticMessage(const QString &message);

public:
    // When enabled (raw16/high precision mode), hide algorithms that are effectively
    // 8-bit-only in this project (same behavior as 8-bit mode).
    void setHighPrecisionMode(bool enabled);
    
private slots:
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void onParametersChanged(const QVariantMap &params);
    void onAlgorithmEnabled(const QString &algorithmId);
    void onAlgorithmDisabled(const QString &algorithmId);
    
private:
    void setupUI();
    void populateTree();
    void updateTreeItemState(const QString &algorithmId, bool enabled);
    
    ImageAlgorithmManager *m_algorithmManager;
    
    QHBoxLayout *m_mainLayout;
    QTreeWidget *m_treeWidget;
    AlgorithmParameterPanel *m_parameterPanel;

    QSplitter *m_splitter{nullptr};
    
    QString m_currentAlgorithmId;

    bool m_highPrecisionMode{false};
    
    // Map tree items to algorithm IDs
    QMap<QTreeWidgetItem*, QString> m_itemToAlgorithmId;
};

#endif // IMAGE_PROCESSING_PANEL_H
