#ifndef PANEL_H
#define PANEL_H

#include <QWidget>

class QListWidget;
class QTimer;

class QmitkDataStorageComboBox;
class QmitkRenderWindow;
class QSlider;

namespace mitk
{
	class DataStorage;
	class TransferFunctionProperty;
}

class Panel : public QWidget
{
	Q_OBJECT

	QmitkDataStorageComboBox *nodecombobox;
	QListWidget *listwidget;
	QmitkRenderWindow *renderwindow;

	QTimer *refreshtimer;

	unsigned char *transferfunctions;
	int nrfunctions;

	double rotateperframe;
	double blendperframe;

	void AddTransferFunctionData(mitk::TransferFunctionProperty *property);
	void AddTransferFunctionItem(const unsigned char *data);

	void RotateCamera(double angle);
	void AdvanceTransferFunctionIndex(double step);

protected:
	void closeEvent(QCloseEvent *event);

protected slots:
	void LoadDataNode();
	void ToggleFullscreen();
	void AddTransferFunction();
	void DeleteTransferFunction();
	void LoadTransferFunctions();
	void SaveTransferFunctions();
	void SetRotationSpeed(int speed);
	void SetTransitionSpeed(int speed);
	void TransferFunctionChanged();
	void Refresh();

public:
	Panel(QWidget *parent = 0, Qt::WindowFlags f = 0);
	~Panel();

public slots:
	void SetDataStorage(mitk::DataStorage *storage);
};
#endif // PANEL_H
