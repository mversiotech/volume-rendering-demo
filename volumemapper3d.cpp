#include "volumemapper3d.h"
#include "opengl.h"
#include "shaderprogram.h"

#include <mitkBaseRenderer.h>
#include <mitkGeometry3D.h>
#include <mitkImage.h>
#include <mitkTransferFunctionProperty.h>
#include <mitkTransferFunction.h>

#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkImageCast.h>
#include <vtkLinearTransform.h>
#include <vtkMatrix4x4.h>
#include <vtkSmartPointer.h>
#include <vtkImageCast.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <QFile>
#include <QByteArray>

VolumeMapper3D::LocalStorage::LocalStorage()
{
	window = NULL;
	windowsize[0] = 0;
	windowsize[1] = 0;

	raysetupprogram = NULL;
	raycastprogram = NULL;
	
	vertexarray = 0;
	
	boundsvertexbuffer = 0;
	boundsindexbuffer = 0;

	quadvertexbuffer = 0;

	frontbackfacefbos[0] = 0;
	frontbackfacefbos[1] = 0;

	frontbackfacetextures[0] = 0;
	frontbackfacetextures[1] = 0;

	volumetexture = 0;
	volumetimestamp = 0;

	transfertexture = 0;
}

VolumeMapper3D::LocalStorage::~LocalStorage()
{
	if (this->window == NULL)
	{
		return;
	}

	this->window->MakeCurrent();

	delete this->raysetupprogram;
	delete this->raycastprogram;

	glDeleteBuffers(1, &this->boundsindexbuffer);
	glDeleteBuffers(1, &this->boundsvertexbuffer);
	glDeleteBuffers(1, &this->quadvertexbuffer);

	glDeleteVertexArrays(1, &this->vertexarray);

	glDeleteFramebuffers(2, &this->frontbackfacefbos[0]);
	glDeleteTextures(2, &this->frontbackfacetextures[0]);

	glDeleteTextures(1, &this->volumetexture);

	glDeleteTextures(1, &this->transfertexture);
}

VolumeMapper3D::VolumeMapper3D() : glinit(false), pendingtexture(NULL),
displaymode(DisplayMode::PREVIEW), transferindex(0.0f)
{
	if (!OpenGL::Init())
	{
		fputs("Can't initialize OpenGL: Volume rendering disabled\n", stderr);
		return;
	}

	if (!OpenGL::VersionSupported(3, 3))
	{
		fputs("OpenGL version 3.3 is not supported on your machine. Volume rendering disabled.\n", stderr);
		return;
	}
	this->glinit = true;
}

VolumeMapper3D::~VolumeMapper3D()
{
	if (this->pendingtexture != NULL)
		this->pendingtexture->Delete();
}

void VolumeMapper3D::SaveWindow(mitk::BaseRenderer *renderer)
{
	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	if (storage->window == NULL)
	{
		storage->window = renderer->GetVtkRenderer()->GetVTKWindow();
	}
}

void VolumeMapper3D::UpdateShaderProgram(mitk::BaseRenderer *renderer, ShaderProgram *&program, const char *vfile, const char *ffile)
{
	// Try to load shader files from the local file system first. This is useful for debugging, because
	// shaders can be loaded and unloaded without recompiling the application. If the specified filenames
	// do not exist, the shader sources are loaded from an Qt resource file (embedded into the executable)
	// instead. Note: SOURCE_PATH is defined in CMakeLists.txt
	static const char *localprefix = SOURCE_PATH "/shader/";
	static const char *internalprefix = ":/shader/";

	if (program != NULL)
		return;

	char vbuffer[MAX_PATH + 1];
	char fbuffer[MAX_PATH + 1];

	sprintf(vbuffer, "%s%s", localprefix, vfile);
	sprintf(fbuffer, "%s%s", localprefix, ffile);

	char *vsrc = ReadFile(vbuffer);
	char *fsrc = ReadFile(fbuffer);

	if (vsrc == NULL || fsrc == NULL)
	{
		delete[]vsrc;
		delete[]fsrc;

		sprintf(vbuffer, "%s%s", internalprefix, vfile);
		sprintf(fbuffer, "%s%s", internalprefix, ffile);

		vsrc = ReadFile(vbuffer);
		fsrc = ReadFile(fbuffer);

		if (vsrc == NULL || fsrc == NULL)
		{
			delete[]vsrc;
			delete[]fsrc;

			fprintf(stderr, "One or more shader files not found: %s, %s\n", vfile, ffile);
			return;
		}
	}

	program = new ShaderProgram();

	if (!program->Build(vsrc, fsrc))
	{
		fprintf(stderr, "Can't build shader program, log follows:\n%s\n", program->GetBuildLog());
		delete program;
		program = NULL;
	}

	delete[]vsrc;
	delete[]fsrc;
}

char *VolumeMapper3D::ReadFile(const char *path, size_t *size)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		fprintf(stderr, "Can't open %s\n", path);
		return NULL;
	}

	char *data = new char[file.size() + 1];
	data[file.size()] = 0;

	size_t bytes = file.read(data, file.size());

	if (bytes != file.size())
	{
		delete[] data;
		return NULL;
	}

	if (size != NULL)
	{
		*size = bytes;
	}

	return data;
}

void VolumeMapper3D::UpdateBoundsVertexBuffer(mitk::BaseRenderer *renderer)
{
	float bounds[6];
	GetBounds(GetDataNode(), bounds);

	float corners[24] = {
		bounds[0], bounds[2], bounds[5], // 0: LBF
		bounds[1], bounds[2], bounds[5], // 1: RBF
		bounds[1], bounds[3], bounds[5], // 2: RTF
		bounds[0], bounds[3], bounds[5], // 3: LTF
		bounds[0], bounds[2], bounds[4], // 4: LBB
		bounds[1], bounds[2], bounds[4], // 5: RBB
		bounds[1], bounds[3], bounds[4], // 6: RTB
		bounds[0], bounds[3], bounds[4]  // 7: LTB
	};

	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	if (storage->boundsvertexbuffer == 0)
		glGenBuffers(1, &storage->boundsvertexbuffer);

	glBindBuffer(GL_ARRAY_BUFFER, storage->boundsvertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(float), corners, GL_STATIC_DRAW);

	if (storage->boundsindexbuffer == 0)
	{
		glGenBuffers(1, &storage->boundsindexbuffer);

		// These indices never change. We only need to upload them once
		uint8_t indices[36] = {
			6, 5, 4,
			7, 6, 4,
			4, 0, 3,
			4, 3, 7,
			2, 1, 5,
			6, 2, 5,
			4, 5, 0,
			5, 1, 0,
			3, 6, 7,
			3, 2, 6,
			0, 1, 2,
			0, 2, 3
		};

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, storage->boundsindexbuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 36 * sizeof(uint8_t), indices, GL_STATIC_DRAW);
	}
}

void VolumeMapper3D::UpdateQuadVertexBuffer(mitk::BaseRenderer *renderer)
{
	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	if (storage->quadvertexbuffer == 0)
		glGenBuffers(1, &storage->quadvertexbuffer);

	float data[] = {
		1.0, 1.0, // vertex position
		1.0, 1.0, // texture coordinate
		-1.0, 1.0,
		0.0, 1.0,
		1.0, -1.0,
		1.0, 0.0,
		-1.0, -1.0,
		0.0, 0.0
	};

	glBindBuffer(GL_ARRAY_BUFFER, storage->quadvertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), data, GL_STATIC_DRAW);
}

void VolumeMapper3D::UpdateFramebufferObjects(mitk::BaseRenderer *renderer)
{
	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	const int w = renderer->GetSizeX();
	const int h = renderer->GetSizeY();

	SaveFramebufferState(renderer);

	for (int i = 0; i < 2; i++)
	{
		if (!glIsFramebuffer(storage->frontbackfacefbos[i]))
		{
			glGenFramebuffers(1, &storage->frontbackfacefbos[i]);
			CheckGLError();
		}

		if (!glIsTexture(storage->frontbackfacetextures[i]))
		{
			glGenTextures(1, &storage->frontbackfacetextures[i]);
			CheckGLError();
		}

		if (w == storage->windowsize[0] && h == storage->windowsize[1])
			continue;

		glBindFramebuffer(GL_FRAMEBUFFER, storage->frontbackfacefbos[i]);

		glBindTexture(GL_TEXTURE_2D, storage->frontbackfacetextures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		CheckGLError();
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, storage->frontbackfacetextures[i], 0);
		CheckGLError();

		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			fprintf(stderr, "Warning: FBO status is 0x%x\n", status);
	}

	storage->windowsize[0] = w;
	storage->windowsize[1] = h;

	RestoreFramebufferState(renderer);
}

void VolumeMapper3D::SaveFramebufferState(mitk::BaseRenderer *renderer)
{
	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	int drawbuffer;
	int readbuffer;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&drawbuffer);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, (GLint*)&readbuffer);

	storage->fbostack.push_back(drawbuffer);
	storage->fbostack.push_back(readbuffer);
}

void VolumeMapper3D::RestoreFramebufferState(mitk::BaseRenderer *renderer)
{
	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	const int size = storage->fbostack.size();

	if (size >= 2)
	{
		int drawbuffer = storage->fbostack[size - 2];
		int readbuffer = storage->fbostack[size - 1];
		storage->fbostack.resize(size - 2);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawbuffer);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, readbuffer);
	}
}


void VolumeMapper3D::GetViewMatrix(mitk::BaseRenderer *renderer, float matrix[16])
{
	vtkCamera *camera = renderer->GetVtkRenderer()->GetActiveCamera();
	vtkMatrix4x4 *view = camera->GetModelViewTransformMatrix();

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			matrix[j * 4 + i] = (float)view->Element[i][j];
		}
	}
}

void VolumeMapper3D::GetProjectionMatrix(mitk::BaseRenderer *renderer, float matrix[16])
{
	vtkCamera *camera = renderer->GetVtkRenderer()->GetActiveCamera();

	double aspect = (double)renderer->GetSizeX() / (double)renderer->GetSizeY();

	double cliprange[2];
	camera->GetClippingRange(cliprange);

	vtkMatrix4x4 *projection = camera->GetProjectionTransformMatrix(aspect, cliprange[0], cliprange[1]);

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			matrix[j * 4 + i] = (float)projection->Element[i][j];
		}
	}

	// Fix for VTK weirdness: https://tinyurl.com/j65pvuh
	const float div = (float)(cliprange[1] - cliprange[0]);
	matrix[10] /= div;
	matrix[14] = (2.0f * matrix[14]) / div;
}

void VolumeMapper3D::GetInverseModelMatrix(mitk::BaseRenderer *renderer, float matrix[16])
{
	mitk::DataNode *node = GetDataNode();
	mitk::Geometry3D *geo = node->GetData()->GetGeometry(renderer->GetTimeStep());

	vtkMatrix4x4 *mxmodel = vtkMatrix4x4::New();
	mxmodel->DeepCopy(geo->GetVtkTransform()->GetMatrix());
	mxmodel->Invert();

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			matrix[j * 4 + i] = (float)mxmodel->Element[i][j];
		}
	}

	mxmodel->Delete();
}

void VolumeMapper3D::GetCameraPosition(mitk::BaseRenderer *renderer, float position[3])
{
	double tmp[3];
	renderer->GetVtkRenderer()->GetActiveCamera()->GetPosition(tmp);

	position[0] = (float)tmp[0];
	position[1] = (float)tmp[1];
	position[2] = (float)tmp[2];
}

void VolumeMapper3D::GetBounds(mitk::DataNode *volume, float bounds[6])
{
	mitk::TimeGeometry *geo = dynamic_cast<mitk::Image*>(volume->GetData())->GetTimeGeometry();
	mitk::BoundingBox *bbox = geo->GetBoundingBoxInWorld();
	bbox->ComputeBoundingBox();

	auto itkbounds = bbox->GetBounds();

	for (int i = 0; i < 6; i++)
	{
		bounds[i] = (float)itkbounds[i];
	}
}

void VolumeMapper3D::GetSpacing(mitk::DataNode *volume, float spacing[3])
{
	double dspacing[3];
	dynamic_cast<mitk::Image*>(volume->GetData())->GetVtkImageData()->GetSpacing(dspacing);

	for (int i = 0; i < 3; i++)
	{
		spacing[i] = (float)dspacing[i];
	}
}

void VolumeMapper3D::UpdateVolumeTexture(mitk::BaseRenderer *renderer)
{
	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	vtkImageData *volume = dynamic_cast<mitk::Image*>(this->GetDataNode()->GetData())->GetVtkImageData();

	uint64_t mtime = volume->GetMTime();

	if (storage->volumetexture != 0 && storage->volumetimestamp == mtime)
	{
		return;
	}

	if (!glIsTexture(storage->volumetexture))
	{
		glGenTextures(1, &storage->volumetexture);
		CheckGLError();
	}

	glBindTexture(GL_TEXTURE_3D, storage->volumetexture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	CheckGLError();

	vtkImageData *normalized = CreateNormalizedVolume(volume);

	int dim[3];
	normalized->GetDimensions(dim);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, dim[0], dim[1], dim[2], 0, GL_RED, GL_FLOAT, normalized->GetScalarPointer());
	CheckGLError();

	glBindTexture(GL_TEXTURE_3D, 0);

	normalized->Delete();

	storage->volumetimestamp = mtime;
}

vtkImageData *VolumeMapper3D::CreateNormalizedVolume(vtkImageData *input)
{
	vtkSmartPointer<vtkImageCast> floatfilter = vtkSmartPointer<vtkImageCast>::New();
	floatfilter->SetOutputScalarTypeToFloat();
	floatfilter->SetInputData(input);
	floatfilter->Update();

	vtkImageData *output = floatfilter->GetOutput();
	output->Register(NULL);

	float *ptr = (float*)output->GetScalarPointer();

	const vtkIdType npts = output->GetNumberOfPoints();

	for (vtkIdType i = 0; i < npts; i++)
	{
		float f = *ptr;
		f += 1024.0f;
		f /= 4096.0f;
		f = std::min(1.0f, std::max(0.0f, f));
		*ptr++ = f;
	}

	return output;
}

void VolumeMapper3D::SetTransferTexture(int nrfunctions, unsigned char *data)
{
	SetTransferFunctionIndex(0.0f);

	if (this->pendingtexture != NULL)
	{
		this->pendingtexture->Delete();
		this->pendingtexture = NULL;
	}

	if (nrfunctions < 1)
		return;

	this->pendingtexture = vtkImageData::New();
	this->pendingtexture->SetDimensions(4096, nrfunctions, 1);
	this->pendingtexture->SetOrigin(0, 0, 0);
	this->pendingtexture->SetSpacing(1, 1, 1);
	this->pendingtexture->AllocateScalars(VTK_UNSIGNED_CHAR, 4);

	memcpy(this->pendingtexture->GetScalarPointer(), data, 4096 * 4 * nrfunctions);
}

void VolumeMapper3D::UpdateTransferTexture(mitk::BaseRenderer *renderer)
{
	if (this->displaymode == DisplayMode::DEMO)
		UpdateTransferTextureDemo(renderer);
	else
		UpdateTransferTexturePreview(renderer);
}
	
void VolumeMapper3D::UpdateTransferTextureDemo(mitk::BaseRenderer *renderer)
{
	if (this->pendingtexture == NULL)
		return;

	int dim[3];
	this->pendingtexture->GetDimensions(dim);

	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	if (!glIsTexture(storage->transfertexture))
		glGenTextures(1, &storage->transfertexture);

	glBindTexture(GL_TEXTURE_2D, storage->transfertexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dim[0], dim[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, this->pendingtexture->GetScalarPointer());
	CheckGLError();

	this->pendingtexture->Delete();
	this->pendingtexture = NULL;
}

void VolumeMapper3D::UpdateTransferTexturePreview(mitk::BaseRenderer *renderer)
{
	mitk::TransferFunctionProperty::Pointer property = mitk::TransferFunctionProperty::New();
	if (!this->GetDataNode()->GetProperty(property, "TransferFunction"))
		return;

	vtkColorTransferFunction *color = property->GetValue()->GetColorTransferFunction();
	vtkPiecewiseFunction *opacity = property->GetValue()->GetScalarOpacityFunction();

	uint8_t *buffer = new uint8_t[4096 * 4];
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
	
	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);
	
	if (!glIsTexture(storage->transfertexture))
		glGenTextures(1, &storage->transfertexture);

	glBindTexture(GL_TEXTURE_2D, storage->transfertexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4096, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	CheckGLError();

	delete[] buffer;
}

void VolumeMapper3D::SetDisplayMode(DisplayMode m)
{
	this->displaymode = m;
}

void VolumeMapper3D::SetTransferFunctionIndex(float index)
{
	this->transferindex = index;
}

float VolumeMapper3D::GetTransferFunctionIndex()
{
	return this->transferindex;
}

void VolumeMapper3D::Paint(mitk::BaseRenderer *renderer)
{
	if (!this->glinit)
		return;

	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	SaveWindow(renderer);
	SaveFramebufferState(renderer);

	if (storage->vertexarray == 0)
		glGenVertexArrays(1, &storage->vertexarray);

	glBindVertexArray(storage->vertexarray);

	// Create or update all texture objects
	UpdateVolumeTexture(renderer);
	UpdateTransferTexture(renderer);

	// Create or update all shaders
	UpdateShaderProgram(renderer, storage->raysetupprogram, "vertex-setup.glsl", "fragment-setup.glsl");
	UpdateShaderProgram(renderer, storage->raycastprogram, "vertex-raycast.glsl", "fragment-raycast.glsl");

	// Create or update all vertex buffers
	UpdateBoundsVertexBuffer(renderer);
	UpdateQuadVertexBuffer(renderer);

	// Create or update all FBOs
	UpdateFramebufferObjects(renderer);

	// Actual draw calls
	RenderBoundingBox(renderer);
	RenderVolume(renderer);

	// Restore everything
	glBindVertexArray(0);
	glUseProgram(0);
	RestoreFramebufferState(renderer);
}

void VolumeMapper3D::RenderBoundingBox(mitk::BaseRenderer *renderer)
{
	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	if (storage->raysetupprogram == NULL)
		return;

	SaveFramebufferState(renderer);

	storage->raysetupprogram->Enable();

	int location = 0;

	float view[16];
	GetViewMatrix(renderer, view);
	location = storage->raysetupprogram->GetUniformLocation("view");
	glUniformMatrix4fv(location, 1, GL_FALSE, view);

	float projection[16];
	GetProjectionMatrix(renderer, projection);
	location = storage->raysetupprogram->GetUniformLocation("projection");
	glUniformMatrix4fv(location, 1, GL_FALSE, projection);

	glBindBuffer(GL_ARRAY_BUFFER, storage->boundsvertexbuffer);
	location = storage->raysetupprogram->GetAttributeLocation("vertex");
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, storage->boundsindexbuffer);

	glEnable(GL_CULL_FACE);

	// Draw front faces only
	glBindFramebuffer(GL_FRAMEBUFFER, storage->frontbackfacefbos[0]);
	glClear(GL_COLOR_BUFFER_BIT);
	glCullFace(GL_BACK);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, NULL);

	// Draw back faces only
	glBindFramebuffer(GL_FRAMEBUFFER, storage->frontbackfacefbos[1]);
	glClear(GL_COLOR_BUFFER_BIT);
	glCullFace(GL_FRONT);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, NULL);

	glDisable(GL_CULL_FACE);

	RestoreFramebufferState(renderer);
}

void VolumeMapper3D::RenderVolume(mitk::BaseRenderer *renderer)
{
	LocalStorage *storage = this->storagehandler.GetLocalStorage(renderer);

	if (storage->raycastprogram == NULL)
		return;

	glClear(GL_COLOR_BUFFER_BIT);

	storage->raycastprogram->Enable();

	int location = 0;

	location = storage->raycastprogram->GetUniformLocation("volume");
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, storage->volumetexture);
	glUniform1i(location, 0);

	location = storage->raycastprogram->GetUniformLocation("frontfaces");
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, storage->frontbackfacetextures[0]);
	glUniform1i(location, 1);

	location = storage->raycastprogram->GetUniformLocation("backfaces");
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, storage->frontbackfacetextures[1]);
	glUniform1i(location, 2);

	location = storage->raycastprogram->GetUniformLocation("transfer");
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, storage->transfertexture);
	glUniform1i(location, 3);

	location = storage->raycastprogram->GetUniformLocation("transferindex");
	glUniform1f(location, this->transferindex);

	float camerapos[3];
	GetCameraPosition(renderer, camerapos);
	location = storage->raycastprogram->GetUniformLocation("camerapos");
	glUniform3fv(location, 1, camerapos);

	float model[16];
	GetInverseModelMatrix(renderer, model);
	location = storage->raycastprogram->GetUniformLocation("invertedmodel");
	glUniformMatrix4fv(location, 1, GL_FALSE, model);

	glBindBuffer(GL_ARRAY_BUFFER, storage->quadvertexbuffer);
	CheckGLError();
	location = storage->raycastprogram->GetAttributeLocation("vertex");
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), NULL);
	CheckGLError();

	location = storage->raycastprogram->GetAttributeLocation("uv");
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	CheckGLError();

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	CheckGLError();

	storage->raycastprogram->Disable();
}
