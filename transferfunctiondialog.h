#ifndef TRANSFER_FUNCTION_DIALOG_H
#define TRANSFER_FUNCTION_DIALOG_H

#include <QDialog>

class QmitkTransferFunctionWidget;

namespace mitk
{
	class DataNode;
}

class TransferFunctionDialog : public QDialog
{
	Q_OBJECT
	QmitkTransferFunctionWidget *transferfunctionwidget;

public:
	TransferFunctionDialog(QWidget *parent = 0, Qt::WindowFlags f = 0);

public slots:
	void SetDataNode(mitk::DataNode *node);
};

#endif // TRANSFER_FUNCTION_DIALOG_H