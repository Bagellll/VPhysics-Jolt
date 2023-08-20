//=================================================================================================
//
// The base physics DLL interface
// 
// This is a thin CPU-agnostic wrapper for the actual Volt DLLs which are named
// vphysics_jolt_sse2.dll, vphysics_jolt_sse42.dll and vphysics_jolt_avx2.dll
//
//=================================================================================================

#include "tier0/basetypes.h"
#include "tier1/interface.h"
#include "vphysics_interface.h"

#include "../vphysics_jolt/compat/branch_overrides.h"

#ifdef _WIN32
#include <intrin.h>
#else
#include <cpuid.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class ISaveRestoreOps;
class IVPhysicsKeyParser;

struct vcollide_t;

//-------------------------------------------------------------------------------------------------

class PhysicsWrapper final : public CBaseAppSystem<IPhysics>
{
public:
	bool Connect( CreateInterfaceFn factory ) override;
	void Disconnect() override;

	InitReturnVal_t Init() override;
	void Shutdown() override;
	void *QueryInterface( const char *pInterfaceName ) override;

	IPhysicsEnvironment *CreateEnvironment() override;
	void DestroyEnvironment( IPhysicsEnvironment *pEnvironment ) override;
	IPhysicsEnvironment *GetActiveEnvironmentByIndex( int index ) override;

	IPhysicsObjectPairHash *CreateObjectPairHash() override;
	void DestroyObjectPairHash( IPhysicsObjectPairHash *pHash ) override;

	IPhysicsCollisionSet *FindOrCreateCollisionSet( unsigned int id, int maxElementCount ) override;
	IPhysicsCollisionSet *FindCollisionSet( unsigned int id ) override;
	void DestroyAllCollisionSets() override;

public:
	static PhysicsWrapper &GetInstance() { return s_PhysicsInterface; }

private:
	bool InitWrapper();

	CSysModule *m_pActualPhysicsModule;
	IPhysics *m_pActualPhysicsInterface;

	static PhysicsWrapper s_PhysicsInterface;
};

PhysicsWrapper PhysicsWrapper::s_PhysicsInterface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( PhysicsWrapper, IPhysics, VPHYSICS_INTERFACE_VERSION, PhysicsWrapper::GetInstance() );

//-------------------------------------------------------------------------------------------------

enum CPULevel_t
{
	CPU_HAS_SSE2,
	CPU_HAS_SSE42,
	CPU_HAS_AVX2,
};

static void GetCPUID( int *pInfo, int func, int subfunc )
{
#ifdef _WIN32
	__cpuidex( pInfo, func, subfunc );
#else
	__cpuid_count( func, subfunc, pInfo[0], pInfo[1], pInfo[2], pInfo[3] );
#endif
}

static CPULevel_t GetCPULevel()
{
	int cpuInfo[4];
	CPULevel_t cpuLevel = CPU_HAS_SSE2;

	GetCPUID( cpuInfo, 0, 0 ); // Get the number of functions
	const int numFuncs = cpuInfo[0];
	if ( numFuncs >= 7 )
	{
		GetCPUID( cpuInfo, 7, 0 ); // Call function 7
		bool hasAVX2 = cpuInfo[1] & ( 1 << 5 ); // 5 is the AVX2 bit
		if ( hasAVX2 )
			cpuLevel = CPU_HAS_AVX2;
	}
	else
	{
		GetCPUID( cpuInfo, 1, 0 ); // Call function 1
		bool hasSSE42 = cpuInfo[2] & ( 1 << 20 ); // 20 is the SSE42 bit
		if ( hasSSE42 )
			cpuLevel = CPU_HAS_SSE42;
	}

	return cpuLevel;
}

static const char *GetModuleFromCPULevel( CPULevel_t level )
{
	switch ( level )
	{
		case CPU_HAS_AVX2:		return "vphysics_jolt_avx2" DLL_EXT_STRING;
		case CPU_HAS_SSE42:		return "vphysics_jolt_sse42" DLL_EXT_STRING;
		default:				return "vphysics_jolt_sse2" DLL_EXT_STRING;
	}
}

// Tries to load the actual vphysics DLL
bool PhysicsWrapper::InitWrapper()
{
	if ( m_pActualPhysicsInterface )
		return true;

	const char *pModuleName = GetModuleFromCPULevel( GetCPULevel() );

	if ( !Sys_LoadInterface( pModuleName, VPHYSICS_INTERFACE_VERSION, &m_pActualPhysicsModule, (void **)&m_pActualPhysicsInterface ) )
		return false;

	return true;
}

bool PhysicsWrapper::Connect( CreateInterfaceFn factory )
{
	if ( !InitWrapper() )
		return false;

	return m_pActualPhysicsInterface->Connect( factory );
}

void PhysicsWrapper::Disconnect()
{
	m_pActualPhysicsInterface->Disconnect();

	Sys_UnloadModule( m_pActualPhysicsModule );
}

//-------------------------------------------------------------------------------------------------

InitReturnVal_t PhysicsWrapper::Init()
{
	return m_pActualPhysicsInterface->Init();
}

void PhysicsWrapper::Shutdown()
{
	m_pActualPhysicsInterface->Shutdown();
}

void *PhysicsWrapper::QueryInterface( const char *pInterfaceName )
{
	// This function can be called before Connect, so try and load the real DLL early
	if ( !InitWrapper() )
		return nullptr;

	return m_pActualPhysicsInterface->QueryInterface( pInterfaceName );
}

//-------------------------------------------------------------------------------------------------

IPhysicsEnvironment *PhysicsWrapper::CreateEnvironment()
{
	return m_pActualPhysicsInterface->CreateEnvironment();
}

void PhysicsWrapper::DestroyEnvironment( IPhysicsEnvironment *pEnvironment )
{
	m_pActualPhysicsInterface->DestroyEnvironment( pEnvironment );
}

IPhysicsEnvironment *PhysicsWrapper::GetActiveEnvironmentByIndex( int index )
{
	return m_pActualPhysicsInterface->GetActiveEnvironmentByIndex( index );
}

//-------------------------------------------------------------------------------------------------

IPhysicsObjectPairHash *PhysicsWrapper::CreateObjectPairHash()
{
	return m_pActualPhysicsInterface->CreateObjectPairHash();
}

void PhysicsWrapper::DestroyObjectPairHash( IPhysicsObjectPairHash *pHash )
{
	m_pActualPhysicsInterface->DestroyObjectPairHash( pHash );
}

//-------------------------------------------------------------------------------------------------

IPhysicsCollisionSet *PhysicsWrapper::FindOrCreateCollisionSet( unsigned int id, int maxElementCount )
{
	return m_pActualPhysicsInterface->FindOrCreateCollisionSet( id, maxElementCount );
}

IPhysicsCollisionSet *PhysicsWrapper::FindCollisionSet( unsigned int id )
{
	return m_pActualPhysicsInterface->FindCollisionSet( id );
}

void PhysicsWrapper::DestroyAllCollisionSets()
{
	m_pActualPhysicsInterface->DestroyAllCollisionSets();
}

//-------------------------------------------------------------------------------------------------

// Wasn't sure if this file should be exposed to a bunch of vjolt headers so I put this here for compatibility
class IJoltPhysicsSurfaceProps : public IPhysicsSurfaceProps
{
public:
#ifndef GAME_PORTAL2_OR_NEWER
	virtual ISaveRestoreOps		*GetMaterialIndexDataOps() const = 0;
#endif

#ifndef GAME_GMOD
	// GMod-specific internal gubbins that was exposed in the public interface.
	virtual void				*GetIVPMaterial( int nIndex ) = 0;
	virtual int					GetIVPMaterialIndex( const void *pMaterial ) const = 0;
	virtual void				*GetIVPManager( void ) = 0;
	virtual int					RemapIVPMaterialIndex( int nIndex ) const = 0;
	virtual const char 			*GetReservedMaterialName( int nMaterialIndex ) const = 0;
#endif
};

class PhysicsSurfacePropsWrapper final : public IJoltPhysicsSurfaceProps
{
public:
	PhysicsSurfacePropsWrapper();
	virtual ~PhysicsSurfacePropsWrapper() override;

	int					ParseSurfaceData( const char *pFilename, const char *pTextfile ) override;
	int					SurfacePropCount( void ) const override;

	int					GetSurfaceIndex( const char *pSurfacePropName ) const override;
	void				GetPhysicsProperties( int surfaceDataIndex, float *density, float *thickness, float *friction, float *elasticity ) const override;

	surfacedata_t		*GetSurfaceData( int surfaceDataIndex ) override;
	const char			*GetString( unsigned short stringTableIndex ) const override;

	const char			*GetPropName( int surfaceDataIndex ) const override;

	void				SetWorldMaterialIndexTable( int *pMapArray, int mapSize ) override;

	void				GetPhysicsParameters( int surfaceDataIndex, surfacephysicsparams_t *pParamsOut ) const override;

	ISaveRestoreOps		*GetMaterialIndexDataOps() const override;

	// GMod-specific internal gubbins that was exposed in the public interface.
	void				*GetIVPMaterial( int nIndex ) override;
	int					GetIVPMaterialIndex( const void *pMaterial ) const override;
	void				*GetIVPManager( void ) override;
	int					RemapIVPMaterialIndex( int nIndex ) const override;
	const char 			*GetReservedMaterialName( int nMaterialIndex ) const override;

public:
	static PhysicsSurfacePropsWrapper &GetInstance() { return s_PhysicsSurfaceProps; }

private:
	CSysModule *m_pActualPhysicsSurfacePropsModule;
	IJoltPhysicsSurfaceProps *m_pActualPhysicsSurfacePropsInterface;

	static PhysicsSurfacePropsWrapper s_PhysicsSurfaceProps;
};

PhysicsSurfacePropsWrapper PhysicsSurfacePropsWrapper::s_PhysicsSurfaceProps = {};
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( PhysicsSurfacePropsWrapper, IPhysicsSurfaceProps, VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, PhysicsSurfacePropsWrapper::GetInstance() );

//-------------------------------------------------------------------------------------------------

PhysicsSurfacePropsWrapper::PhysicsSurfacePropsWrapper()
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	if ( m_pActualPhysicsSurfacePropsInterface ) return;

	const char *pModuleName = GetModuleFromCPULevel( GetCPULevel() );

	if ( !Sys_LoadInterface( pModuleName, VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, &m_pActualPhysicsSurfacePropsModule, (void **)&m_pActualPhysicsSurfacePropsInterface ) )
		Msg( "Failed to load %s\n", pModuleName );
}

PhysicsSurfacePropsWrapper::~PhysicsSurfacePropsWrapper()
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	Sys_UnloadModule( m_pActualPhysicsSurfacePropsModule );
}

//-------------------------------------------------------------------------------------------------

int PhysicsSurfacePropsWrapper::ParseSurfaceData( const char *pFilename, const char *pTextfile )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->ParseSurfaceData( pFilename, pTextfile );
}

int PhysicsSurfacePropsWrapper::SurfacePropCount( void ) const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->SurfacePropCount();
}

//-------------------------------------------------------------------------------------------------

int PhysicsSurfacePropsWrapper::GetSurfaceIndex( const char *pSurfacePropName ) const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->GetSurfaceIndex( pSurfacePropName );
}

void PhysicsSurfacePropsWrapper::GetPhysicsProperties( int surfaceDataIndex, float *density, float *thickness, float *friction, float *elasticity ) const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsSurfacePropsInterface->GetPhysicsProperties( surfaceDataIndex, density, thickness, friction, elasticity );
}

//-------------------------------------------------------------------------------------------------

surfacedata_t *PhysicsSurfacePropsWrapper::GetSurfaceData( int surfaceDataIndex )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->GetSurfaceData( surfaceDataIndex );
}

const char *PhysicsSurfacePropsWrapper::GetString( unsigned short stringTableIndex ) const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->GetString( stringTableIndex );
}

//-------------------------------------------------------------------------------------------------

const char *PhysicsSurfacePropsWrapper::GetPropName( int surfaceDataIndex ) const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->GetPropName( surfaceDataIndex );
}

//-------------------------------------------------------------------------------------------------

void PhysicsSurfacePropsWrapper::SetWorldMaterialIndexTable( int *pMapArray, int mapSize )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsSurfacePropsInterface->SetWorldMaterialIndexTable( pMapArray, mapSize );
}

//-------------------------------------------------------------------------------------------------

void PhysicsSurfacePropsWrapper::GetPhysicsParameters( int surfaceDataIndex, surfacephysicsparams_t *pParamsOut ) const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsSurfacePropsInterface->GetPhysicsParameters( surfaceDataIndex, pParamsOut );
}

//-------------------------------------------------------------------------------------------------

ISaveRestoreOps *PhysicsSurfacePropsWrapper::GetMaterialIndexDataOps() const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->GetMaterialIndexDataOps();
}

//-------------------------------------------------------------------------------------------------

void *PhysicsSurfacePropsWrapper::GetIVPMaterial( int nIndex )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->GetIVPMaterial( nIndex );
}

int PhysicsSurfacePropsWrapper::GetIVPMaterialIndex( const void *pMaterial ) const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->GetIVPMaterialIndex( pMaterial );
}

void *PhysicsSurfacePropsWrapper::GetIVPManager( void )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->GetIVPManager();
}

int PhysicsSurfacePropsWrapper::RemapIVPMaterialIndex( int nIndex ) const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->RemapIVPMaterialIndex( nIndex );
}

const char *PhysicsSurfacePropsWrapper::GetReservedMaterialName( int nMaterialIndex ) const
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsSurfacePropsInterface->GetReservedMaterialName( nMaterialIndex );
}

//-------------------------------------------------------------------------------------------------

class IJoltPhysicsCollision : public IPhysicsCollision
{
public:
	using IPhysicsCollision::VPhysicsKeyParserCreate;
#ifndef GAME_ASW_OR_NEWER
	virtual IVPhysicsKeyParser	*VPhysicsKeyParserCreate( vcollide_t *pVCollide ) = 0;

	virtual float				CollideGetRadius( const CPhysCollide *pCollide ) = 0;

	virtual void				*VCollideAllocUserData( vcollide_t *pVCollide, size_t userDataSize ) = 0;
	virtual void				VCollideFreeUserData( vcollide_t *pVCollide ) = 0;
	virtual void				VCollideCheck( vcollide_t *pVCollide, const char *pName ) = 0;
#endif

#ifndef GAME_CSGO_OR_NEWER
	virtual bool				TraceBoxAA( const Ray_t &ray, const CPhysCollide *pCollide, trace_t *ptr ) = 0;

	virtual void				DuplicateAndScale( vcollide_t *pOut, const vcollide_t *pIn, float flScale ) = 0;
#endif
};

class PhysicsCollisionWrapper final : public IJoltPhysicsCollision
{
public:
	PhysicsCollisionWrapper();
	virtual ~PhysicsCollisionWrapper() override;

	CPhysConvex		*ConvexFromVerts( Vector **pVerts, int vertCount ) override;
	CPhysConvex		*ConvexFromPlanes( float *pPlanes, int planeCount, float mergeDistance ) override;
	float			ConvexVolume( CPhysConvex *pConvex ) override;

	float			ConvexSurfaceArea( CPhysConvex *pConvex ) override;
	void			SetConvexGameData( CPhysConvex *pConvex, unsigned int gameData ) override;
	void			ConvexFree( CPhysConvex *pConvex ) override;
	CPhysConvex		*BBoxToConvex( const Vector &mins, const Vector &maxs ) override;
	CPhysConvex		*ConvexFromConvexPolyhedron( const CPolyhedron &ConvexPolyhedron ) override;
	void			ConvexesFromConvexPolygon( const Vector &vPolyNormal, const Vector *pPoints, int iPointCount, CPhysConvex **pOutput ) override;

	CPhysPolysoup	*PolysoupCreate() override;
	void			PolysoupDestroy( CPhysPolysoup *pSoup ) override;
	void			PolysoupAddTriangle( CPhysPolysoup *pSoup, const Vector &a, const Vector &b, const Vector &c, int materialIndex7bits ) override;
	CPhysCollide	*ConvertPolysoupToCollide( CPhysPolysoup *pSoup, bool useMOPP ) override;
	
	CPhysCollide	*ConvertConvexToCollide( CPhysConvex **pConvex, int convexCount ) override;
	CPhysCollide	*ConvertConvexToCollideParams( CPhysConvex **pConvex, int convexCount, const convertconvexparams_t &convertParams ) override;
	void			DestroyCollide( CPhysCollide *pCollide ) override;

	int				CollideSize( CPhysCollide *pCollide ) override;
	int				CollideWrite( char *pDest, CPhysCollide *pCollide, bool bSwap = false ) override;
	CPhysCollide	*UnserializeCollide( char *pBuffer, int size, int index ) override;

	float			CollideVolume( CPhysCollide *pCollide ) override;
	float			CollideSurfaceArea( CPhysCollide *pCollide ) override;

	Vector			CollideGetExtent( const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, const Vector &direction ) override;

	void			CollideGetAABB( Vector *pMins, Vector *pMaxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles ) override;

	void			CollideGetMassCenter( CPhysCollide *pCollide, Vector *pOutMassCenter ) override;
	void			CollideSetMassCenter( CPhysCollide *pCollide, const Vector &massCenter ) override;
	Vector			CollideGetOrthographicAreas( const CPhysCollide *pCollide ) override;
	void			CollideSetOrthographicAreas( CPhysCollide *pCollide, const Vector &areas ) override;

	int				CollideIndex( const CPhysCollide *pCollide ) override;

	CPhysCollide	*BBoxToCollide( const Vector &mins, const Vector &maxs ) override;
	int				GetConvexesUsedInCollideable( const CPhysCollide *pCollideable, CPhysConvex **pOutputArray, int iOutputArrayLimit ) override;


	void			TraceBox( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr ) override;
	void			TraceBox( const Ray_t &ray, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr ) override;
	void			TraceBox( const Ray_t &ray, unsigned int contentsMask, IConvexInfo *pConvexInfo, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr ) override;

	void			TraceCollide( const Vector &start, const Vector &end, const CPhysCollide *pSweepCollide, const QAngle &sweepAngles, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr ) override;

	bool			IsBoxIntersectingCone( const Vector &boxAbsMins, const Vector &boxAbsMaxs, const truncatedcone_t &cone ) override;

	void			VCollideLoad( vcollide_t *pOutput, int solidCount, const char *pBuffer, int size, bool swap = false ) override;
	void			VCollideUnload( vcollide_t *pVCollide ) override;

	IVPhysicsKeyParser	*VPhysicsKeyParserCreate( const char *pKeyData ) override;
	IVPhysicsKeyParser	*VPhysicsKeyParserCreate( vcollide_t *pVCollide ) override;
	void				VPhysicsKeyParserDestroy( IVPhysicsKeyParser *pParser ) override;

	int				CreateDebugMesh( CPhysCollide const *pCollisionModel, Vector **outVerts ) override;
	void			DestroyDebugMesh( int vertCount, Vector *outVerts ) override;

	ICollisionQuery *CreateQueryModel( CPhysCollide *pCollide ) override;
	void			DestroyQueryModel( ICollisionQuery *pQuery ) override;

	IPhysicsCollision *ThreadContextCreate() override;
	void			ThreadContextDestroy( IPhysicsCollision *pThreadContex ) override;

	CPhysCollide	*CreateVirtualMesh( const virtualmeshparams_t &params ) override;
	bool			SupportsVirtualMesh() override;


	bool			GetBBoxCacheSize( int *pCachedSize, int *pCachedCount ) override;

	
	CPolyhedron		*PolyhedronFromConvex( CPhysConvex * const pConvex, bool bUseTempPolyhedron ) override;

	void			OutputDebugInfo( const CPhysCollide *pCollide ) override;
	unsigned int	ReadStat( int statID ) override;

	float			CollideGetRadius( const CPhysCollide *pCollide ) override;

	void			*VCollideAllocUserData( vcollide_t *pVCollide, size_t userDataSize ) override;
	void			VCollideFreeUserData( vcollide_t *pVCollide ) override;
	void			VCollideCheck( vcollide_t *pVCollide, const char *pName ) override;

	bool			TraceBoxAA( const Ray_t &ray, const CPhysCollide *pCollide, trace_t *ptr ) override;

	void			DuplicateAndScale( vcollide_t *pOut, const vcollide_t *pIn, float flScale ) override;
public:
	static PhysicsCollisionWrapper &GetInstance() { return s_PhysicsCollision; }

private:
	CSysModule *m_pActualPhysicsCollisionModule;
	IJoltPhysicsCollision *m_pActualPhysicsCollisionInterface;

	static PhysicsCollisionWrapper s_PhysicsCollision;
};

PhysicsCollisionWrapper PhysicsCollisionWrapper::s_PhysicsCollision = {};
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( PhysicsCollisionWrapper, IPhysicsCollision, VPHYSICS_COLLISION_INTERFACE_VERSION, PhysicsCollisionWrapper::GetInstance() );

//-------------------------------------------------------------------------------------------------

PhysicsCollisionWrapper::PhysicsCollisionWrapper()
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	if ( m_pActualPhysicsCollisionInterface ) return;

	const char *pModuleName = GetModuleFromCPULevel( GetCPULevel() );

	if ( !Sys_LoadInterface( pModuleName, VPHYSICS_COLLISION_INTERFACE_VERSION, &m_pActualPhysicsCollisionModule, (void **)&m_pActualPhysicsCollisionInterface ) )
		Msg( "Failed to load %s\n", pModuleName );
}

PhysicsCollisionWrapper::~PhysicsCollisionWrapper()
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	Sys_UnloadModule( m_pActualPhysicsCollisionModule );
}

CPhysConvex *PhysicsCollisionWrapper::ConvexFromVerts( Vector **pVerts, int vertCount )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ConvexFromVerts( pVerts, vertCount );
}

CPhysConvex *PhysicsCollisionWrapper::ConvexFromPlanes( float *pPlanes, int planeCount, float mergeDistance )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ConvexFromPlanes( pPlanes, planeCount, mergeDistance );
}

float PhysicsCollisionWrapper::ConvexVolume( CPhysConvex *pConvex )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ConvexVolume( pConvex );
}

//-------------------------------------------------------------------------------------------------

float PhysicsCollisionWrapper::ConvexSurfaceArea( CPhysConvex *pConvex )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ConvexSurfaceArea( pConvex );
}

void PhysicsCollisionWrapper::SetConvexGameData( CPhysConvex *pConvex, unsigned int gameData )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->SetConvexGameData( pConvex, gameData );
}

void PhysicsCollisionWrapper::ConvexFree( CPhysConvex *pConvex )
{
	m_pActualPhysicsCollisionInterface->ConvexFree( pConvex );
}

CPhysConvex *PhysicsCollisionWrapper::BBoxToConvex( const Vector &mins, const Vector &maxs )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->BBoxToConvex( mins, maxs );
}

CPhysConvex *PhysicsCollisionWrapper::ConvexFromConvexPolyhedron( const CPolyhedron &ConvexPolyhedron )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ConvexFromConvexPolyhedron( ConvexPolyhedron );
}

void PhysicsCollisionWrapper::ConvexesFromConvexPolygon( const Vector &vPolyNormal, const Vector *pPoints, int iPointCount, CPhysConvex **pOutput )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->ConvexesFromConvexPolygon( vPolyNormal, pPoints, iPointCount, pOutput );
}

//-------------------------------------------------------------------------------------------------

CPhysPolysoup *PhysicsCollisionWrapper::PolysoupCreate()
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->PolysoupCreate();
}

void PhysicsCollisionWrapper::PolysoupDestroy( CPhysPolysoup *pSoup )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->PolysoupDestroy( pSoup );
}

void PhysicsCollisionWrapper::PolysoupAddTriangle( CPhysPolysoup *pSoup, const Vector &a, const Vector &b, const Vector &c, int materialIndex7bits )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->PolysoupAddTriangle( pSoup, a, b, c, materialIndex7bits );
}

CPhysCollide *PhysicsCollisionWrapper::ConvertPolysoupToCollide( CPhysPolysoup *pSoup, bool useMOPP )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ConvertPolysoupToCollide( pSoup, useMOPP );
}

//-------------------------------------------------------------------------------------------------

CPhysCollide *PhysicsCollisionWrapper::ConvertConvexToCollide( CPhysConvex **pConvex, int convexCount )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ConvertConvexToCollide( pConvex, convexCount );
}

CPhysCollide *PhysicsCollisionWrapper::ConvertConvexToCollideParams( CPhysConvex **pConvex, int convexCount, const convertconvexparams_t &convertParams )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ConvertConvexToCollideParams( pConvex, convexCount, convertParams );
}

void PhysicsCollisionWrapper::DestroyCollide( CPhysCollide *pCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->DestroyCollide( pCollide );
}

//-------------------------------------------------------------------------------------------------

int PhysicsCollisionWrapper::CollideSize( CPhysCollide *pCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CollideSize( pCollide );
}

int PhysicsCollisionWrapper::CollideWrite( char *pDest, CPhysCollide *pCollide, bool bSwap )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CollideWrite( pDest, pCollide, bSwap );
}

CPhysCollide *PhysicsCollisionWrapper::UnserializeCollide( char *pBuffer, int size, int index )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->UnserializeCollide( pBuffer, size, index );
}

//-------------------------------------------------------------------------------------------------

float PhysicsCollisionWrapper::CollideVolume( CPhysCollide *pCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CollideVolume( pCollide );
}

float PhysicsCollisionWrapper::CollideSurfaceArea( CPhysCollide *pCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CollideSurfaceArea( pCollide );
}

//-------------------------------------------------------------------------------------------------

Vector PhysicsCollisionWrapper::CollideGetExtent( const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, const Vector &direction )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CollideGetExtent( pCollide, collideOrigin, collideAngles, direction );
}

//-------------------------------------------------------------------------------------------------

void PhysicsCollisionWrapper::CollideGetAABB( Vector *pMins, Vector *pMaxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->CollideGetAABB( pMins, pMaxs, pCollide, collideOrigin, collideAngles );
}

//-------------------------------------------------------------------------------------------------

void PhysicsCollisionWrapper::CollideGetMassCenter( CPhysCollide *pCollide, Vector *pOutMassCenter )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->CollideGetMassCenter( pCollide, pOutMassCenter );
}

void PhysicsCollisionWrapper::CollideSetMassCenter( CPhysCollide *pCollide, const Vector &massCenter )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->CollideSetMassCenter( pCollide, massCenter );
}

Vector PhysicsCollisionWrapper::CollideGetOrthographicAreas( const CPhysCollide *pCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CollideGetOrthographicAreas( pCollide );
}

void PhysicsCollisionWrapper::CollideSetOrthographicAreas( CPhysCollide *pCollide, const Vector &areas )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->CollideSetOrthographicAreas( pCollide, areas );
}

//-------------------------------------------------------------------------------------------------

int PhysicsCollisionWrapper::CollideIndex( const CPhysCollide *pCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CollideIndex( pCollide );
}

//-------------------------------------------------------------------------------------------------

CPhysCollide *PhysicsCollisionWrapper::BBoxToCollide( const Vector &mins, const Vector &maxs )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->BBoxToCollide( mins, maxs );
}

int PhysicsCollisionWrapper::GetConvexesUsedInCollideable( const CPhysCollide *pCollideable, CPhysConvex **pOutputArray, int iOutputArrayLimit )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->GetConvexesUsedInCollideable( pCollideable, pOutputArray, iOutputArrayLimit );
}

//-------------------------------------------------------------------------------------------------

void PhysicsCollisionWrapper::TraceBox( const Vector &start, const Vector &end, const Vector &mins, const Vector &maxs, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->TraceBox( start, end, mins, maxs, pCollide, collideOrigin, collideAngles, ptr );
}

void PhysicsCollisionWrapper::TraceBox( const Ray_t &ray, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->TraceBox( ray, pCollide, collideOrigin, collideAngles, ptr );
}

void PhysicsCollisionWrapper::TraceBox( const Ray_t &ray, unsigned int contentsMask, IConvexInfo *pConvexInfo, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->TraceBox( ray, contentsMask, pConvexInfo, pCollide, collideOrigin, collideAngles, ptr );
}

//-------------------------------------------------------------------------------------------------

void PhysicsCollisionWrapper::TraceCollide( const Vector &start, const Vector &end, const CPhysCollide *pSweepCollide, const QAngle &sweepAngles, const CPhysCollide *pCollide, const Vector &collideOrigin, const QAngle &collideAngles, trace_t *ptr )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->TraceCollide( start, end, pSweepCollide, sweepAngles, pCollide, collideOrigin, collideAngles, ptr );
}

//-------------------------------------------------------------------------------------------------

bool PhysicsCollisionWrapper::IsBoxIntersectingCone( const Vector &boxAbsMins, const Vector &boxAbsMaxs, const truncatedcone_t &cone )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->IsBoxIntersectingCone( boxAbsMins, boxAbsMaxs, cone );
}

//-------------------------------------------------------------------------------------------------

void PhysicsCollisionWrapper::VCollideLoad( vcollide_t *pOutput, int solidCount, const char *pBuffer, int size, bool swap )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->VCollideLoad( pOutput, solidCount, pBuffer, size, swap );
}

void PhysicsCollisionWrapper::VCollideUnload( vcollide_t *pVCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->VCollideUnload( pVCollide );
}

//-------------------------------------------------------------------------------------------------

IVPhysicsKeyParser *PhysicsCollisionWrapper::VPhysicsKeyParserCreate( const char *pKeyData )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->VPhysicsKeyParserCreate( pKeyData );
}

IVPhysicsKeyParser *PhysicsCollisionWrapper::VPhysicsKeyParserCreate( vcollide_t *pVCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->VPhysicsKeyParserCreate( pVCollide );
}

void PhysicsCollisionWrapper::VPhysicsKeyParserDestroy( IVPhysicsKeyParser *pParser )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->VPhysicsKeyParserDestroy( pParser );
}

//-------------------------------------------------------------------------------------------------

int PhysicsCollisionWrapper::CreateDebugMesh( CPhysCollide const *pCollisionModel, Vector **outVerts )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CreateDebugMesh( pCollisionModel, outVerts );
}

void PhysicsCollisionWrapper::DestroyDebugMesh( int vertCount, Vector *outVerts )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->DestroyDebugMesh( vertCount, outVerts );
}

//-------------------------------------------------------------------------------------------------

ICollisionQuery *PhysicsCollisionWrapper::CreateQueryModel( CPhysCollide *pCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CreateQueryModel( pCollide );
}

void PhysicsCollisionWrapper::DestroyQueryModel( ICollisionQuery *pQuery )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->DestroyQueryModel( pQuery );
}

//-------------------------------------------------------------------------------------------------

IPhysicsCollision *PhysicsCollisionWrapper::ThreadContextCreate()
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ThreadContextCreate();
}

void PhysicsCollisionWrapper::ThreadContextDestroy( IPhysicsCollision *pThreadContex )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->ThreadContextDestroy( pThreadContex );
}

//-------------------------------------------------------------------------------------------------

CPhysCollide *PhysicsCollisionWrapper::CreateVirtualMesh( const virtualmeshparams_t &params )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CreateVirtualMesh( params );
}

bool PhysicsCollisionWrapper::SupportsVirtualMesh()
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->SupportsVirtualMesh();
}

//-------------------------------------------------------------------------------------------------

bool PhysicsCollisionWrapper::GetBBoxCacheSize( int *pCachedSize, int *pCachedCount )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->GetBBoxCacheSize( pCachedSize, pCachedCount );
}

//-------------------------------------------------------------------------------------------------

CPolyhedron *PhysicsCollisionWrapper::PolyhedronFromConvex( CPhysConvex * const pConvex, bool bUseTempPolyhedron )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->PolyhedronFromConvex( pConvex, bUseTempPolyhedron );
}

//-------------------------------------------------------------------------------------------------

void PhysicsCollisionWrapper::OutputDebugInfo( const CPhysCollide *pCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->OutputDebugInfo( pCollide );
}

unsigned int PhysicsCollisionWrapper::ReadStat( int statID )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->ReadStat( statID );
}

//-------------------------------------------------------------------------------------------------

float PhysicsCollisionWrapper::CollideGetRadius( const CPhysCollide *pCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->CollideGetRadius( pCollide );
}

//-------------------------------------------------------------------------------------------------

void *PhysicsCollisionWrapper::VCollideAllocUserData( vcollide_t *pVCollide, size_t userDataSize )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->VCollideAllocUserData( pVCollide, userDataSize );
}

void PhysicsCollisionWrapper::VCollideFreeUserData( vcollide_t *pVCollide )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->VCollideFreeUserData( pVCollide );
}

void PhysicsCollisionWrapper::VCollideCheck( vcollide_t *pVCollide, const char *pName )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->VCollideCheck( pVCollide, pName );
}

//-------------------------------------------------------------------------------------------------

bool PhysicsCollisionWrapper::TraceBoxAA( const Ray_t &ray, const CPhysCollide *pCollide, trace_t *ptr )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	return m_pActualPhysicsCollisionInterface->TraceBoxAA( ray, pCollide, ptr );
}

//-------------------------------------------------------------------------------------------------

void PhysicsCollisionWrapper::DuplicateAndScale( vcollide_t *pOut, const vcollide_t *pIn, float flScale )
{
	Msg("Debug: Entering %s\n", __FUNCTION__);
	m_pActualPhysicsCollisionInterface->DuplicateAndScale( pOut, pIn, flScale );
}

//-------------------------------------------------------------------------------------------------


