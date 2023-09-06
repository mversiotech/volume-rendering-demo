// Own stuff
#include "panel.h"
#include "transferfunctiondialog.h"
#include "volumemapper3d.h"

// Standard library
#include <stdint.h>

// Qt
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QSlider>
#include <QPainter>
#include <QListWidget>
#include <QSettings>
#include <QApplication>
#include <QCloseEvent>
#include <QTimer>

// Qmitk
#include <QmitkDataStorageComboBox.h>
#include <QmitkTransferFunctionWidget.h>
#include <QmitkRenderWindow.h>

// MITK
#include <mitkNodePredicateDataType.h>
#include <mitkDataNodeFactory.h>
#include <mitkTransferFunction.h>
#include <mitkTransferFunctionProperty.h>

// VTK
#include <vtkColorTransferFunction.h>
#include <vtkCamera.h>

Panel::Panel(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f)
{
	this->transferfunctions = NULL;
	this->nrfunctions = 0;
	this->rotateperframe = 0.0;
	this->blendperframe = 0.0;

	this->setMinimumSize(250, 630);

	QVBoxLayout *panellayout = new QVBoxLayout();
	this->setLayout(panellayout);

	panellayout->addWidget(new QLabel(tr("Select node")));

	this->nodecombobox = new QmitkDataStorageComboBox();
	this->nodecombobox->SetPredicate(mitk::NodePredicateDataType::New("Image"));
	panellayout->addWidget(this->nodecombobox);

	QPushButton *openbutton = new QPushButton(tr("Open..."));
	connect(openbutton, SIGNAL(clicked()), this, SLOT(LoadDataNode()));
	panellayout->addWidget(openbutton);

	panellayout->addSpacing(12);

	panellayout->addWidget(new QLabel(tr("Transfer functions")));

	this->listwidget = new QListWidget();
	this->listwidget->setMinimumSize(256, 256);
	this->listwidget->setIconSize(QSize(256, 32));
	panellayout->addWidget(this->listwidget);

	QHBoxLayout *tfaddremovelayout = new QHBoxLayout();

	QPushButton *addtfbutton = new QPushButton(tr("Add..."));
	connect(addtfbutton, SIGNAL(clicked()), this, SLOT(AddTransferFunction()));
	tfaddremovelayout->addWidget(addtfbutton);
	QPushButton *deltfbutton = new QPushButton(tr("Remove"));
	connect(deltfbutton, SIGNAL(clicked()), this, SLOT(DeleteTransferFunction()));
	tfaddremovelayout->addWidget(deltfbutton);
	panellayout->addLayout(tfaddremovelayout);

	panellayout->addSpacing(6);

	QHBoxLayout *tfiolayout = new QHBoxLayout();
	QPushButton *loadtfbutton = new QPushButton(tr("Load..."));
	connect(loadtfbutton, SIGNAL(clicked()), this, SLOT(LoadTransferFunctions()));
	tfiolayout->addWidget(loadtfbutton);
	QPushButton *savetfbutton = new QPushButton(tr("Save..."));
	connect(savetfbutton, SIGNAL(clicked()), this, SLOT(SaveTransferFunctions()));
	tfiolayout->addWidget(savetfbutton);
	panellayout->addLayout(tfiolayout);

	panellayout->addSpacing(12);

	panellayout->addWidget(new QLabel(tr("Rotation speed")));

	QSlider *rotationslider = new QSlider();
	rotationslider->setOrientation(Qt::Horizontal);
	rotationslider->setRange(0, 99);
	rotationslider->setSingleStep(1);
	connect(rotationslider, SIGNAL(valueChanged(int)), this, SLOT(SetRotationSpeed(int)));
	panellayout->addWidget(rotationslider);

	panellayout->addSpacing(12);

	panellayout->addWidget(new QLabel(tr("Transfer function transition speed")));
	
	QSlider *transitionslider = new QSlider();
	transitionslider->setOrientation(Qt::Horizontal);
	transitionslider->setRange(0, 99);
	transitionslider->setSingleStep(1);
	connect(transitionslider, SIGNAL(valueChanged(int)), this, SLOT(SetTransitionSpeed(int)));
	panellayout->addWidget(transitionslider);

	panellayout->addSpacing(12);

	QPushButton *renderbutton = new QPushButton(tr("Toggle fullscreen"));
	connect(renderbutton, SIGNAL(clicked()), this, SLOT(ToggleFullscreen()));
	panellayout->addWidget(renderbutton);

	panellayout->addStretch(1);
	
	this->renderwindow = new QmitkRenderWindow();
	this->renderwindow->GetRenderer()->SetMapperID(mitk::BaseRenderer::Standard3D);
	this->renderwindow->show();
	this->renderwindow->resize(512, 512);

	this->renderwindow->setAttribute(Qt::WA_DeleteOnClose);
	connect(this->renderwindow, SIGNAL(destroyed(QObject*)), QApplication::instance(), SLOT(quit()));

	this->refreshtimer = new QTimer(this);
	connect(this->refreshtimer, SIGNAL(timeout()), this, SLOT(Refresh()));
}

Panel::~Panel()
{
	delete[] this->transferfunctions;
}
	
void Panel::closeEvent(QCloseEvent *event)
{
	QApplication::closeAllWindows();
	event->accept();
}

void Panel::SetDataStorage(mitk::DataStorage *storage)
{
	this->nodecombobox->SetDataStorage(storage);
	this->renderwindow->GetRenderer()->SetDataStorage(storage);
}

void Panel::LoadDataNode()
{
	QSettings settings;
	QString lastpath(settings.value("Panel/LastDataNode").toString());

	QString filename = QFileDialog::getOpenFileName(this, tr("Load image"), lastpath, "Known extensions (*.nrrd *.dcm *.mhd *.nii *.nhdr);;All files (*)");

	if (filename.isEmpty())
		return;

	QByteArray local = filename.toLocal8Bit();

	try
	{
		mitk::DataNodeFactory::Pointer reader = mitk::DataNodeFactory::New();
		reader->SetFileName(local.constData());
		reader->Update();
		mitk::DataNode::Pointer node = reader->GetOutput();

		VolumeMapper3D::Pointer mapper = VolumeMapper3D::New();
		mapper->SetDisplayMode(VolumeMapper3D::DisplayMode::DEMO);
		node->SetMapper(mitk::BaseRenderer::Standard3D, mapper);

		mitk::DataStorage::Pointer storage = this->nodecombobox->GetDataStorage();
		storage->Add(node);

		mitk::TimeGeometry::Pointer geo = storage->ComputeBoundingGeometry3D(storage->GetAll());
		mitk::RenderingManager::GetInstance()->InitializeViews(geo);
	}
	catch (...)
	{
		QMessageBox mbox;
		mbox.setText(tr("Couldn't load ") + filename);
		mbox.setInformativeText(tr("Either the resource is not accessible, or the file format is not understood by this application."));
		mbox.setIcon(QMessageBox::Warning);
		mbox.exec();
		return;
	}

	settings.setValue("Panel/LastDataNode", filename);
}

void Panel::AddTransferFunction()
{
	mitk::DataNode *node = this->nodecombobox->GetSelectedNode();

	if (node == NULL)
	{
		QMessageBox mbox;
		mbox.setText(tr("Load a data node first."));
		mbox.setInformativeText(tr("The transfer function editor can only be used after an image has been loaded"));
		mbox.setIcon(QMessageBox::Warning);
		mbox.exec();
		return;
	}
	
	VolumeMapper3D *mapper = dynamic_cast<VolumeMapper3D*>(node->GetMapper(mitk::BaseRenderer::Standard3D));
	mapper->SetDisplayMode(VolumeMapper3D::DisplayMode::PREVIEW);
	this->refreshtimer->stop();

	TransferFunctionDialog dialog(this);
	dialog.SetDataNode(node);

	int ret = dialog.exec();

	mapper->SetDisplayMode(VolumeMapper3D::DisplayMode::DEMO);
	this->refreshtimer->start(33);
	
	if (ret != QDialog::Accepted)
		return;	

	mitk::TransferFunctionProperty::Pointer property = mitk::TransferFunctionProperty::New();
	if (!node->GetProperty(property, "TransferFunction"))
	{
		fputs("No transfer function property found\n", stderr);
		return;
	}

	AddTransferFunctionData(property);

	const uint8_t *data = this->transferfunctions;
	data += 4096 * 4 * (this->nrfunctions - 1);
	AddTransferFunctionItem(data);

	TransferFunctionChanged();
}
	
void Panel::AddTransferFunctionData(mitk::TransferFunctionProperty *property)
{	
	uint8_t *buffer = new uint8_t[4096 * 4 * (this->nrfunctions + 1)];
	if (this->nrfunctions != 0)
	{
		memcpy(buffer, this->transferfunctions, 4096 * 4 * this->nrfunctions);
	}
	this->transferfunctions = buffer;

	vtkColorTransferFunction *color = property->GetValue()->GetColorTransferFunction();
	vtkPiecewiseFunction *opacity = property->GetValue()->GetScalarOpacityFunction();

	buffer += 4096 * 4 * this->nrfunctions;
	double rgba[4];

	for (int i = 0; i < 4096; i++)
	{
		color->GetColor(i - 1024, rgba);
		rgba[3] = opacity->GetValue(i - 1024);

		rgba[0] *= rgba[3];
		rgba[1] *= rgba[3];
		rgba[2] *= rgba[3];

		buffer[i * 4 + 0] = (uint8_t)(rgba[0] * 255.0);
		buffer[i * 4 + 1] = (uint8_t)(rgba[1] * 255.0);
		buffer[i * 4 + 2] = (uint8_t)(rgba[2] * 255.0);
		buffer[i * 4 + 3] = (uint8_t)(rgba[3] * 255.0);
	}
	
	this->nrfunctions++;
}
	
void Panel::AddTransferFunctionItem(const unsigned char *data)
{
	QImage tmp(4096, 512, QImage::Format_ARGB32_Premultiplied);
	QPainter painter(&tmp);

	for (int x = 0; x < 4096; x++)
	{
		int r = data[x * 4 + 0];
		int g = data[x * 4 + 1];
		int b = data[x * 4 + 2];
		int a = data[x * 4 + 3];

		if (a == 0)
			continue;
		
		r = (255 * r) / a;
		g = (255 * g) / a;
		b = (255 * b) / a;

		painter.setPen(QColor(r, g, b));
		painter.drawLine(x, 511 - a * 2, x, 511);
	}
	painter.end();

	QListWidgetItem *item = new QListWidgetItem();
	item->setIcon(QIcon(QPixmap::fromImage(tmp.scaled(256, 32))));
	item->setSizeHint(QSize(256, 32));

	this->listwidget->addItem(item);
}

void Panel::DeleteTransferFunction()
{
	int index = this->listwidget->currentRow();

	if (index < 0 || index >= this->nrfunctions)
		return;

	delete this->listwidget->takeItem(index);

	uint8_t *buffer = new uint8_t[4096 * 4 * (this->nrfunctions - 1)];
	memcpy(buffer, this->transferfunctions, 4 * 4096 * index);
	memcpy(buffer + 4 * 4096 * index, this->transferfunctions + 4 * 4096 * (index + 1), 4 * 4096 * (this->nrfunctions - index - 1));

	delete[] this->transferfunctions;
	this->transferfunctions = buffer;

	this->nrfunctions--;
}

void Panel::LoadTransferFunctions()
{
	QSettings settings;
	QString lastpath(settings.value("Panel/LastTransferFunction").toString());

	QString filename = QFileDialog::getOpenFileName(this, tr("Open"), lastpath, "PNG images (*.png)");
	if (filename.isEmpty())
		return;

	QImage image;
	if (!image.load(filename))
	{
		QMessageBox mbox;
		mbox.setText(tr("Couldn't load ") + filename);
		mbox.setInformativeText(tr("Either the resource is not accessible, or the file format is not understood by this application."));
		mbox.setIcon(QMessageBox::Warning);
		mbox.exec();
		return;
	}

	if (image.width() != 4096)
	{
		QMessageBox mbox;
		mbox.setText(tr("Couldn't load ") + filename);
		mbox.setInformativeText(tr("Transfer function images are expected to be exactly 4096 pixels wide"));
		mbox.setIcon(QMessageBox::Warning);
		mbox.exec();
		return;
	}

	this->nrfunctions = 0;
	delete[] this->transferfunctions;
	this->transferfunctions = NULL;
	this->listwidget->clear();

	if (image.format() != QImage::Format_ARGB32_Premultiplied)
		image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);

	this->transferfunctions = new uint8_t[4096 * 4 * image.height()];
	memcpy(this->transferfunctions, image.bits(), 4096 * 4 * image.height());

	this->nrfunctions = image.height();

	for (int i = 0; i < this->nrfunctions; i++)
	{
		const uint8_t *data = this->transferfunctions + 4096 * 4 * i;
		AddTransferFunctionItem(data);
	}

	settings.setValue("Panel/LastTransferFunction", filename);

	TransferFunctionChanged();

	this->refreshtimer->start(33);
}

void Panel::SaveTransferFunctions()
{
	if (this->nrfunctions < 1)
		return;
	
	QSettings settings;
	QString lastpath(settings.value("Panel/LastTransferFunction").toString());

	QString filename = QFileDialog::getSaveFileName(this, tr("Save as"), lastpath, "PNG images (*.png)");

	if (filename.isEmpty())
		return;

	QImage image(this->transferfunctions, 4096, this->nrfunctions, QImage::Format_ARGB32_Premultiplied);
	
	if (!image.save(filename))
	{
		QMessageBox mbox;
		mbox.setText(tr("Couldn't write to ") + filename);
		mbox.setIcon(QMessageBox::Warning);
		mbox.exec();
		return;
	}

	settings.setValue("Panel/LastTransferFunction", filename);
}

void Panel::SetRotationSpeed(int speed)
{
	double scaled = (double)speed / 100.0;
	this->rotateperframe = 5.0 * scaled;
}

void Panel::SetTransitionSpeed(int speed)
{
	double scaled = (double)speed / 100.0;
	this->blendperframe = 0.01 * scaled;
}

void Panel::TransferFunctionChanged()
{
	mitk::DataNode *node = this->nodecombobox->GetSelectedNode();
	if (node == NULL)
		return;

	VolumeMapper3D *mapper = dynamic_cast<VolumeMapper3D*>(node->GetMapper(mitk::BaseRenderer::Standard3D));
	if (mapper == NULL)
		return;

	mapper->SetTransferTexture(this->nrfunctions, this->transferfunctions);

	mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void Panel::ToggleFullscreen()
{
	if (this->renderwindow->isFullScreen())
		this->renderwindow->showNormal();
	else
		this->renderwindow->showFullScreen();
}

void Panel::RotateCamera(double angle)
{
	mitk::DataNode *node = this->nodecombobox->GetSelectedNode();

	if (node == NULL || angle == 0.0)
		return;

	this->renderwindow->GetRenderer()->GetVtkRenderer()->GetActiveCamera()->Azimuth(angle);
}
	
void Panel::AdvanceTransferFunctionIndex(double step)
{
	mitk::DataNode *node = this->nodecombobox->GetSelectedNode();

	if (node == NULL || step == 0.0 || this->nrfunctions < 2)
		return;

	VolumeMapper3D *mapper = static_cast<VolumeMapper3D*>(node->GetMapper(mitk::BaseRenderer::Standard3D));

	double index = (double)mapper->GetTransferFunctionIndex();
	index += step;

	if (index >= (double)this->nrfunctions)
	{
		index = index - floor(index);
	}

	mapper->SetTransferFunctionIndex((float)index);
}

void Panel::Refresh()
{
	if (this->nrfunctions < 1)
	{
		this->refreshtimer->stop();
		return;
	}

	RotateCamera(this->rotateperframe);
	AdvanceTransferFunctionIndex(this->blendperframe);

	mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}
