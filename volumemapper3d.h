#ifndef VOLUMEMAPPER3D_H
#define VOLUMEMAPPER3D_H

#include <mitkGLMapper.h>
#include <mitkCoreServices.h>

class ShaderProgram;

class vtkImageData;
class vtkWindow;

class VolumeMapper3D : public mitk::GLMapper
{
	VolumeMapper3D(const VolumeMapper3D &);
	VolumeMapper3D &operator=(const VolumeMapper3D &);

public:
	mitkClassMacro(VolumeMapper3D, mitk::GLMapper);
	itkFactorylessNewMacro(Self);
	itkCloneMacro(Self);

	enum DisplayMode {
		PREVIEW, DEMO
	};

	class LocalStorage
	{
	public:
		LocalStorage();
		~LocalStorage();

		vtkWindow *window;
		int windowsize[2];

		ShaderProgram *raysetupprogram;
		ShaderProgram *raycastprogram;
		
		unsigned int vertexarray;
		
		unsigned int boundsvertexbuffer;
		unsigned int boundsindexbuffer;

		unsigned int quadvertexbuffer;

		unsigned int frontbackfacefbos[2];
		unsigned int frontbackfacetextures[2];

		unsigned int volumetexture;
		uint64_t volumetimestamp;

		unsigned int transfertexture;

		std::vector<int> fbostack;
	};

	void SetTransferTexture(int nrfunctions, unsigned char *data);
	void SetTransferFunctionIndex(float index);
	float GetTransferFunctionIndex();
	void SetDisplayMode(DisplayMode m);
	void Paint(mitk::BaseRenderer *renderer);

protected:
	VolumeMapper3D();
	virtual ~VolumeMapper3D();

	mitk::LocalStorageHandler<LocalStorage> storagehandler;
	bool glinit;
	vtkImageData *pendingtexture;
	DisplayMode displaymode;
	float transferindex;

	void SaveWindow(mitk::BaseRenderer *renderer);

	void UpdateShaderProgram(mitk::BaseRenderer *renderer, ShaderProgram *&program, const char *vfile, const char *ffile);

	void UpdateVolumeTexture(mitk::BaseRenderer *renderer);

	vtkImageData *CreateNormalizedVolume(vtkImageData *input);

	void UpdateTransferTexture(mitk::BaseRenderer *renderer);
	void UpdateTransferTextureDemo(mitk::BaseRenderer *renderer);
	void UpdateTransferTexturePreview(mitk::BaseRenderer *renderer);

	// ReadFile opens a file or embedded Qt resource and returns its whole contents as an
	// null-terminated array of bytes. If parameter size is not NULL, the total number of bytes
	// read will be written to it. The returned array must be deleted by the caller. 
	// If an error occurs, ReadFile returns NULL.
	char *ReadFile(const char *path, size_t *size = NULL);

	void UpdateBoundsVertexBuffer(mitk::BaseRenderer *renderer);
	void UpdateQuadVertexBuffer(mitk::BaseRenderer *renderer);

	void UpdateFramebufferObjects(mitk::BaseRenderer *renderer);

	void SaveFramebufferState(mitk::BaseRenderer *renderer);
	void RestoreFramebufferState(mitk::BaseRenderer *renderer);

	void RenderBoundingBox(mitk::BaseRenderer *renderer);
	void RenderVolume(mitk::BaseRenderer *renderer);

	void GetViewMatrix(mitk::BaseRenderer *renderer, float matrix[16]);
	void GetProjectionMatrix(mitk::BaseRenderer *renderer, float matrix[16]);
	void GetInverseModelMatrix(mitk::BaseRenderer *renderer, float matrix[16]);
	void GetCameraPosition(mitk::BaseRenderer *renderer, float position[3]);

	// GetBounds retrieves the bounding box coordinates for a given volume.
	// Output order: xmin, xmax, ymin, ymax, zmin, zmax
	// All values are given in world coordinates.
	void GetBounds(mitk::DataNode *volume, float bounds[6]);

	// GetSpacing retrieves the spacing, i.e. the physical dimensions of a single voxel, for a given volume.
	void GetSpacing(mitk::DataNode *volume, float spacing[3]);
};

#endif // VOLUMEMAPPER3D_H