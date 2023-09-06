#include "transferfunctiondialog.h"

#include <QmitkTransferFunctionWidget.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QGroupBox>

TransferFunctionDialog::TransferFunctionDialog(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f)
{
	QVBoxLayout *mainlayout = new QVBoxLayout();
	this->setLayout(mainlayout);

	this->transferfunctionwidget = new QmitkTransferFunctionWidget();
	this->transferfunctionwidget->SetGradientOpacityFunctionEnabled(false);
	this->transferfunctionwidget->ShowGradientOpacityFunction(false);
	mainlayout->addWidget(this->transferfunctionwidget);

	mainlayout->addSpacing(24);

	QHBoxLayout *buttonlayout = new QHBoxLayout();
	buttonlayout->addStretch(1);

	QPushButton *acceptbutton = new QPushButton(tr("OK"));
	connect(acceptbutton, SIGNAL(clicked()), this, SLOT(accept()));
	buttonlayout->addWidget(acceptbutton);

	QPushButton *cancelbutton = new QPushButton(tr("Cancel"));
	connect(cancelbutton, SIGNAL(clicked()), this, SLOT(reject()));
	buttonlayout->addWidget(cancelbutton);

	mainlayout->addLayout(buttonlayout);
}

void TransferFunctionDialog::SetDataNode(mitk::DataNode *node)
{
	node->SetBoolProperty("volumerendering", true);

	this->transferfunctionwidget->SetDataNode(node);
}
