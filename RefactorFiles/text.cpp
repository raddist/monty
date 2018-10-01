////////////////////////////////////////////////////////////////////////////////
//
// #include <k3dobj01\shell3d.h>
//
////////////////////////////////////////////////////////////////////////////////
#include <03100_Obj3D/03100_obj3D_pch.h>  

#include <03100_Obj3D/03100_obj3D_rc2.h>
#include <03100_Obj3D/03100_obj3D_RC_bas.h>

#include <FSys/KompasAppID.h>

#include <ObjS/Warnarr.h>
#include <ObjS/ToleranceReader.h>

#include <solid.h>
#include <cur_line3d.h>
#include <cur_line_segment.h>
#include <surf_plane.h>
#include <action.h>

#include <K3dObj01/RESULTOP.H>
#include <K3dObj01/IFEROLS1.H>
#include <K3dObj01/DetailSolid.h>
#include <K3dObj01/SKOPPARE.H>
#include <K3dObj01/ResultDe.h>
#include <K3dObj01/ShMtVariables.h>
#include <K3dObj01/IfCopibleObj.h>
#include <K3dObj01/ShMtBendUtil.H>
#include <K3dObj01/ShMtBend.H>
#include "K3dObj01/HotPoint.h"
#include "K3dObj01/IfHotPoint.h"
#include "K3dObj01/HotPointBendOper.h"
#include <K3dObj01/D3PRTYPE.H>
#include <K3dObj01/transif.h>
#include <K3dObj01/ShMtCombinedBend.H>

#include <K3dObj01/GenerativeDimensionUtils.h>
#include <K3dObj01/D3AngleDimension.h>
#include "PartObjCopy.h"
#include "K3dObj01/GenerativeDimensionDescriptor.h"
#include "K3dObj01/IfGenerativeDimension.h"

#include <math_namespace.h>
#include <last.h>

using namespace c3d;

extern HINSTANCE module;

// ����������� ������ ��� �������� ���� � ���� �� �����
/*
���������� ������ �� ����� ����������/������������
�������� � PrepareLocalSolid, ���� ��� ���� ������� �� ������
*/
static CreatorForBendUnfoldParams * creatorBends = nullptr;
// ����������� ������ ��� �������� ��������
static CreatorForBendHookUnfoldParams * creatorBendHooks = nullptr;

////////////////////////////////////////////////////////////////////////////////
//
// ������ �������� ����(�������)
//
////////////////////////////////////////////////////////////////////////////////
ShMtBaseBend::ShMtBaseBend( IfUniqueName * un )
  : PartOperation( un ),
  ShMtStraighten(),
  MultiRefConteinerOperation(),
  m_bendObject( 1 )
{
  m_sourceColor.SetAvailableUseCase( SourceColor::EVariantsUseCase::parentHand );
}


//------------------------------------------------------------------------------
// ����������� ������������
// ---
ShMtBaseBend::ShMtBaseBend( const ShMtBaseBend & other )
  : PartOperation( other ),
  ShMtStraighten( other ),
  MultiRefConteinerOperation( other ),
  m_bendObject( other.m_bendObject )
{
  m_sourceColor.SetAvailableUseCase( SourceColor::EVariantsUseCase::parentHand );
}


//------------------------------------------------------------------------------
//
// ---
ShMtBaseBend::ShMtBaseBend( TapeInit )
  : PartOperation( tapeInit ),
  ShMtStraighten(),
  MultiRefConteinerOperation(),
  m_bendObject( 1 )
{
  m_sourceColor.SetAvailableUseCase( SourceColor::EVariantsUseCase::parentHand );
}


IMP_CLASS_DESC_FUNC( kompasDeprecatedAppID, ShMtBaseBend );


//------------------------------------------------------------------------------
//
// ---
IfSomething * ShMtBaseBend::QueryInterface( unsigned int iid ) const
{
  switch ( iid )
  {
  case iidr_ShMtBaseBend:
    AddRef();
    return static_cast<IfShMtBaseBend *>(const_cast<ShMtBaseBend*>(this));
  default:
  {
    IfSomething * res = ShMtStraighten::QueryInterface( iid );
    if ( !res )
      res = SheetMetalOperation::QueryInterface( iid );
    if ( !res )
      res = MultiRefConteinerOperation::QueryInterface( iid );
    if ( !res )
      res = PartOperation::QueryInterface( iid );
    return res;
  }
  }

  return nullptr;
}


I_IMP_SOMETHING_FUNCS_AR( ShMtBaseBend )


bool ShMtBaseBend::IsThisParent( uint8 relType, const WrapSomething & parent ) const {
  return m_bendObject.IsThisParent( rlt_Strong, parent );
}


//------------------------------------------------------------------------------
// ���������� ��������� �� ������� �������
//---
void ShMtBaseBend::Assign( const PrObject &other ) {
  PartOperation::Assign( other );

  const ShMtBaseBend * oper = TYPESAFE_DOWNCAST( &other, const ShMtBaseBend );
  if ( oper ) {
    m_bendObject.Init( oper->m_bendObject );
  }

  AssignStraightenOper( other );

  const MultiRefConteinerOperation * surf = TYPESAFE_DOWNCAST( &other, const MultiRefConteinerOperation );
  if ( surf )
    MultiRefConteinerOperation::AssignSurf( *surf );
}


//-----------------------------------------------------------------------------
/// �������� ���������� ��� ���������
//---
void ShMtBaseBend::RenameUnique( IfUniqueName & un )
{
  PartOperation::RenameUnique( un );
  ShMtStraighten::RenameUnique( un );
}


//------------------------------------------------------------------------------
//
// ---
bool ShMtBaseBend::IsModified( ModifiedPar & par ) const
{
  bool operationModified = PartOperation::IsModified( par );
  bool anyBendModified = IsModifiedAnyBend();

  return operationModified || anyBendModified;
}


//------------------------------------------------------------------------------
// ������������� �� �����-���� �� ����������, ��������� � �����������
// ---
bool ShMtBaseBend::IsAnyVariableParameterModified() const
{
  return PartOperation::IsDrivingParametersModified() || IsModifiedAnyBend();
}

//------------------------------------------------------------------------------
// ������ ������ �����
// ---
void ShMtBaseBend::SetObject( const WrapSomething * wrapper ) {
  m_bendObject.SetObject( 0, wrapper );
}


//------------------------------------------------------------------------------
// �������� ������ �����
// ---
const WrapSomething & ShMtBaseBend::GetObject() const {
  return m_bendObject.GetObjectTrans( 0 );
}


//------------------------------------------------------------------------------
/**
������ ������������ ����� ����������
*/
//---
bool ShMtBaseBend::IsObjectExist( const WrapSomething * bend ) const
{
  return bend && m_bendObject.IsObjectExist( *bend );
}


//------------------------------------------------------------------------------
// �������� �������������� ��������
// ---
bool ShMtBaseBend::IsValid() const {
  return PartOperation::IsValid() && !m_bendObject.IsEmpty() && IsValidBends();
}


//-----------------------------------------------------------------------------
// �������� ���������� ����� ���� ��� ??
//---
bool ShMtBaseBend::IsPossibleMultiSolid() const {
  return true;
}


//------------------------------------------------------------------------------
// ������������ �����
// ---
bool ShMtBaseBend::RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath )
{
  bool res = PartOperation::RestoreReferences( result, allRefs, withMath );
  bool resObj = m_bendObject.RestoreReferences( result, allRefs, withMath );

  return res && resObj;
} // RestoreReferences


  //------------------------------------------------------------------------------
  // �������� ������� ������� soft = true - ������ ������� (�� ������ �����)
  // ---
void ShMtBaseBend::ClearReferencedObjects( bool soft )
{
  PartOperation::ClearReferencedObjects( soft );
  m_bendObject.ClearObjects( soft );
}


//------------------------------------------------------------------------------
// �������� ������ �� �������
// ---
bool ShMtBaseBend::ChangeReferences( ChangeReferencesData & changedRefs )
{
  return m_bendObject.ChangeReferences( changedRefs );
}


//------------------------------------------------------------------------------
// ��������� ���������� � �������� Solida ��� ��������� �������
// ---
bool ShMtBaseBend::MakePhantom( IfComponent & compOwner )
{
  if ( InitPhantom( compOwner, true ) )
    return true;

  ClearVEF();
  return false;
}


//------------------------------------------------------------------------------
//
// ---
bool ShMtBaseBend::InitPhantom( const IfComponent &operOwner, bool setNames )
{
  return InitPhantomPrevSolid( operOwner, tct_Modified/*changed*/, true/*setNames*/, true );
}


//------------------------------------------------------------------------------
// �������� ������ �������������� �� �������
// ---
void ShMtBaseBend::WhatsWrong( WarningsArray & warnings ) {
  PartOperation::WhatsWrong( warnings );

  if ( m_bendObject.IsEmpty() )
    m_bendObject.WhatsWrong( warnings );

  WhatsWrongBends( warnings );
}


//------------------------------------------------------------------------------
//
// ---
IfFeature::ItemStateFlags ShMtBaseBend::GetNodeState() const
{
  return ShMtStraighten::GetNodeState();
}


//------------------------------------------------------------------------------
// ����� ������ �������� � ������
// ---
void ShMtBaseBend::GetBmpKeys( short int &k1, short int &k2 ) const {
  k1 = ::hash( ::pureName( GetTypeInfo().name() ) );
  k2 = (short int)(IsStraighten() ? 1 : 0);
}


//------------------------------------------------------------------------------
// ������ ����� VEF'��
// ---
void ShMtBaseBend::ResetPrimitives( const IfComponent &compOwner )
{
  PartOperation::ResetPrimitives( compOwner ); // ������� ���� VEF
  InitSheetMetalFaces( compOwner, GetPartOperationResult(), MainId() );

  ResetPrimitivesInSubBends( compOwner );
}


//------------------------------------------------------------------------------
/**
��������� ���� ���� ����������
\param set -
*/
//--- 
void ShMtBaseBend::SetMathFlag( bool set )
{
  PartOperation::SetMathFlag( set );
  SetMathFlagInSubBends( set );
}


//------------------------------------------------------------------------------
/**
����� ��������� ���� ����������
*/
//--- 
void ShMtBaseBend::SetVEFZeroOwner()
{
  PartOperation::SetVEFZeroOwner();
  SetVEFZeroOwnerInSubBends();
}



//------------------------------------------------------------------------------
/**
����������� ��������
\param lastResult -
*/
//--- 
void ShMtBaseBend::Rebuild( PartRebuildResult & lastResult )
{
  PartOperation::Rebuild( lastResult );
  IncrementUpdateStampInSubBends();
}


//------------------------------------------------------------------------------
// 
// ---
IfFeatureEnum * ShMtBaseBend::GetSubFeatures( bool direct, bool t, IfFeature * comp ) {
  IfFeatureEnum * fEnum = PartOperation::GetSubFeatures( direct, t, comp );
  return ShMtStraighten::GetSubFeatures( fEnum );
}


//------------------------------------------------------------------------------
// 
// ---
unsigned int ShMtBaseBend::HasSubFeatures( IfFeature * comp )
{
  return ShMtStraighten::HasSubFeatures( comp );
}


//------------------------------------------------------------------------------
// ������ ������ ����������, ��������� � �����������
// ---
void ShMtBaseBend::GetVariableParameters( VarParArray & parVars, ParCollType parCollType )
{
  PartOperation::GetVariableParameters( parVars, parCollType );
  ShMtStraighten::GetVariableParameters( parVars, parCollType );
}


//------------------------------------------------------------------------------
// �������� ����� ��������������� ����������
//---
void ShMtBaseBend::ClearModifiedFlags( VarParArray & array ) // �������� �������� ����������� � ����������
                                                             //SA K9 31.10.2006 void ShMtBaseBend::ClearModifiedFlags( FDPArray<VarParameter> & array ) 
{
  PartOperation::ClearModifiedFlags( array );
  ShMtStraighten::ClearModifiedFlags( array );
}


//------------------------------------------------------------------------------
// ��������� ������
// ---
void ShMtBaseBend::AdditionalReadingEnd( PartRebuildResult & lastResult )
{
  PartOperation::AdditionalReadingEnd( lastResult );
  UpdateMeasurePars( gtt_none, true );

  AdditionalReadingEndInSubBends( lastResult );
}


//------------------------------------------------------------------------------
/**
�������� ������
*/
//--- 
void ShMtBaseBend::ReadingEnd()
{
  PartOperation::ReadingEnd();

  ReadingEndInSubBends();
}


//------------------------------------------------------------------------------------
/// 
/**
// \method    GetSubParOwnerEnum
// \access    virtual private
// \return    std::auto_ptr<PointersIterator<IfVariableParametersOwner>>
// \qualifier const
*/
//---
std::auto_ptr<PointersIterator<VariableParametersOwner>> ShMtBaseBend::GetSubParOwnerEnum() const
{
  return std::auto_ptr<PointersIterator<VariableParametersOwner>>( new PointersArrayIterator<IFC_Array<ShMtBendObject>, VariableParametersOwner>( m_bendObjects ) );
}


//------------------------------------------------------------------------------
// ������ �� ������
// ---
void ShMtBaseBend::Read( reader &in, ShMtBaseBend * obj )
{
  ReadBase( in, static_cast<PartOperation *>(obj) );

  if ( in.AppVersion() < 0x11000033L )
  {
    ReferenceContainerFix bendObject( 1 );
    in >> bendObject;
    // ������ �� ����� ���� ����� ������ �����, ������� ������� ���, � ����� ���� ����������� �������
    obj->m_bendObject.Reset();
    obj->m_bendObject.Add( bendObject[0]->Duplicate() );
  }
  else
    in >> obj->m_bendObject;

  ReadStraightenOper( in, obj );
  obj->ReadSurf( in, 0x0800011AL );
}


//------------------------------------------------------------------------------
// ������ � �����
// ---
void ShMtBaseBend::Write( writer &out, const ShMtBaseBend * obj ) {
  if ( out.AppVersion() < 0x06000033L )
    out.setState( io::cantWriteObject ); // ���������� ��������� ��� ��������� ��������
  else {
    WriteBase( out, static_cast<const PartOperation *>(obj) );

    if ( out.AppVersion() < 0x11000033L )
    {
      ReferenceContainerFix bendObject( 1 );
      bendObject.SetObject( 0, obj->GetBendObject() );
      out << bendObject;
    }
    else
      out << obj->m_bendObject;

    WriteStraightenOper( out, obj );

    // �� �8+ ��������������
    if ( out.AppVersion() >= 0x0800011AL )
      obj->WriteSurf( out );
  }
}


////////////////////////////////////////////////////////////////////////////////
//
// ����� �������� ����
//
////////////////////////////////////////////////////////////////////////////////
class ShMtBend : public IfShMtBend,
  public ShMtBaseBend,
  public IfShMtCircuitBend,
  public IfCopibleObject,
  public IfCopibleOperation,
  public IfCopibleObjectOnRefs,
  public IfCopibleOperationOnEdges,
  public IfHotPoints,
  public IfCanConvert,
  public RefContainersOwner
{
private:
  enum TypeCircuitBend
  {
    tcb_AdjoiningBetween = 0x1, // ��������� ������� �����
    tcb_AdjoiningBeginEnd = 0x2, // ��������� ������� � ������
  };

  typedef uint TypeCircuitBendFlag;

private:
  mutable TypeCircuitBendFlag m_typeCircuitBend;

public:

  ShMtBendParameters     m_params;     ///< ��������� ��������
  ReferenceContainerFix  m_refObjects; ///< ������ �� ������������� �������

public:
  /// �����������, ������ ���������� ��������� ������ �� �����������, ����� ����� �����
  /// ����������� ��� ������������� � ������������ ��������
  ShMtBend( const ShMtBendParameters &, IfUniqueName * un );
  /// ����������� �����������
  ShMtBend( const ShMtBend & );

public:
  I_DECLARE_SOMETHING_FUNCS;

  virtual bool                              IsObjectExist( const WrapSomething * bend ) const { return ShMtBaseBend::IsObjectExist( bend ); }
  virtual double                            GetOwnerAbsAngle() const override;  ///
  virtual double                            GetOwnerValueBend() const override;  ///
  virtual double                            GetOwnerAbsRadius() const override;  /// ���������� ������ �����
  virtual bool                              IsOwnerInternalRad() const override;  /// �� ����������� �������?
  virtual UnfoldType                        GetOwnerTypeUnfold() const override;  /// ���� ��� ���������  
  virtual void                              SetOwnerTypeUnfold( UnfoldType t ) override; /// ������ ��� ���������  
  virtual bool                              GetOwnerFlagByBase() const override;  /// 
  virtual void                              SetOwnerFlagByBase( bool val ) override; /// 
  virtual double                            GetOwnerCoefficient() const override;  /// �����������
  virtual bool                              IfOwnerBaseChanged( IfComponent * trans ) override; /// ���� ���������� �����-���� ���������, � ���� ������ ���� �� ����, �� ��������� ���� 
  virtual double                            GetThickness() const override;   /// ���� �������
  virtual void                              SetThickness( double ) override; /// ������ �������      

                                                                             //IfShMtCircuitBend
                                                                             /// ����� �� ������������ �������� ������� �����
  virtual       bool            IsCanoutAdjoiningBetween() const override;
  /// ����� �� ������������ �������� �������
  virtual       bool            IsCanoutAdjoiningBegin() override;
  /// ����� �� ������������ �������� ������
  virtual       bool            IsCanoutAdjoiningEnd() override;
  virtual       bool            IsClosedPath() const  override;
  virtual ShMtCircuitBendParameters & GetCircuitParameters() override { return m_params; }


  //------------------------------------------------------------------------------
  // IfSheetMetalOperation
  // ����� ��� ��� ����������� ��������������
  virtual void                              EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp ) override;

  //------------------------------------------------------------------------------
  // IfRefContainersOwner
  virtual void GetRefContainers( FDPArray<AbsReferenceContainer> & ) override;

  //------------------------------------------------------------------------------
  // ���������� IfShMtBend
  virtual void                              SetParameters( const ShMtBaseBendParameters & ) override;
  virtual ShMtBaseBendParameters          & GetParameters() const override;
  virtual void                              SetBendObject( const WrapSomething * ) override;
  virtual const WrapSomething &             GetBendObject() const override;

#ifdef MANY_THICKNESS_BODY_SHMT
  // ������� �� ��������� ������
  virtual bool                              IsSuitedObject( const WrapSomething & ) override;
#else
  virtual bool                              IsSuitedObject( const WrapSomething &, double ) override;
#endif

  virtual bool                              IsValid() const override;  // �������� �������������� �������
                                                                       //---------------------------------------------------------------------------
                                                                       // IfCopibleObject
                                                                       // ������� ����� �������
  virtual IfPartObjectCopy * CreateObjectCopy( IfComponent & compOwner ) const override;
  // ������� ���������� �� ��������� ����������
  virtual bool               NeedCopyOnComp( const IfComponent & component ) const override;
  virtual bool               IsAuxGeom() const override { return false; } ///< ��� ��������������� ���������?
                                                                          /// ����� �� �������� ������� �� ���������� � �������
  virtual bool               CanCopyByVarsTable() const override { return false; }

  virtual bool               AddBendObject( const WrapSomething & ) override;
  virtual bool               RemoveBendObject( size_t index ) override;
  virtual void               FlushBendObject() override;
  virtual WrapSomething      GetBend( size_t i ) const override;
  virtual size_t             GetBendCount() const override;
  virtual bool               IsMultiBend() const override;


  //---------------------------------------------------------------------------
  // IfCopibleOperation
  // ����� �� ���������� ��� �������� ���-������ �����, ����� ��� �������������?
  // �.�. ����� MakePureSolid ��� ����� ���������� IfCopibleOperationOnPlaces,
  // IfCopibleOperationOnEdges � �.�.
  virtual bool                          CanCopyNonGeometrically() const override;

  //---------------------------------------------------------------------------
  // IfCopibleObjectOnRefs
  // ������ ������ �� �������, ������ ������� ����� ����� �� ��������� ����������
  virtual const AbsReferenceContainer * GetReferencesToFind( const IfComponent & comp ) override;

  //---------------------------------------------------------------------------
  // IfCopibleOperationOnEdges
  // ���� ���������� ������, ������� ��� ����������� ������ ���� �������� �� �����
  virtual bool  GetInternalReferences( AbsReferenceContainer & internalRefs ) override;
  // ��������� �������� ��� ��������� �����.
  virtual uint  MakeSolidOnEdges( MakeSolidParam        & mksdParam,
    const MbPlacement3D   & copyPlace,
    FDPArray<MbCurveEdge> & foundEdges,
    EdgesPairs        *     internalEdgesPairs,
    SimpleName              mainId,
    bool                    needInvert,
    MbSolid              *& resultSolid,
    PArray<MbPositionData> * posData ) override;

  //------------------------------------------------------------------------------
  // ����� ������� ������� ������ 
  virtual uint            GetObjType() const override;                            // ������� ��� �������
  virtual uint            GetObjClass() const override;                            // ������� ����� ��� �������
  virtual uint            OperationSubType() const override;   // ������� ������ ��������
  virtual void            Assign( const PrObject &other ) override;           // ���������� ��������� �� ������� �������
  virtual PrObject      & Duplicate() const override;

  virtual void            WhatsWrong( WarningsArray & ); // �������� ������ �������������� �� �������
  virtual void            GetVariableParameters( VarParArray &, ParCollType parCollType ) override;
  virtual void            ForgetVariables(); // �� K6+ ������ ��� ����������, ��������� � �����������
  virtual void            AdditionalReadingEnd( PartRebuildResult & prr ) override; /// ����� ������ ReadingEnd
                                                                                    /// ������������ �����
  virtual bool            RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath ) override;
  virtual void            ClearReferencedObjects( bool soft ) override;
  virtual bool            ChangeReferences( ChangeReferencesData & changedRefs ) override; ///< �������� ������ �� �������
  virtual bool            IsThisParent( uint8 relType, const WrapSomething & ) const override;
  /// �������� ���������� ��� ���������
  virtual void            RenameUnique( IfUniqueName & un ) override;

  virtual bool            PrepareToAdding( PartRebuildResult &, PartObject * ) override; //������������� �� ����������� � ������
  virtual bool            InitPhantom( const IfComponent &, bool /*setNames*/ ) override;
  // �������� ���������� ��� ��������������� Solid
  // ��� ������ ���������� ����������� �������������������
  virtual MbSolid       * MakeSolid( uint              & codeError,
    MakeSolidParam    & mksdParam,
    uint                shellThreadId ) override;

  virtual void            PrepareToBuild( const IfComponent & ) override; ///< ������������� � ����������
  virtual void            PostBuild() override; ///< ����� �������� ����� ����������
  virtual bool            CopyInnerProps( const IfPartObject& source,
    uint sourceSubObjectIndex,
    uint destSubObjectIndex ) override;

  /// ���������� ���������� ��� ��������������� ����
  MbSolid       * PrepareLocalSolid( std::vector<MbCurveEdge*> edges,
    uint              & codeError,
    MakeSolidParam    & mksdParam,
    uint                shellThreadId,
    SimpleName          mainId );
  /// ������� �� ��������� ������
  bool            IsSuitedObject( const MbCurveEdge &, double );
  /// ���� �� ��������� ��� ������ ��� �������
  virtual bool            IsNeedSaveAsUnhistored( long version ) const override;
  virtual void            ConvertTo5_11( IfCanConvert & owner, const WritingParms & wrParms ) override;
  virtual void            ConvertToSavePrevious( SaveDocumentVersion saveMode, IfCanConvert & owner, const WritingParms & wrParms ) override;
  virtual IfCanConvert *  CreateConverter( IfCanConvert & what ) override;
  virtual bool            IsNeedConvertTo5_11( SSArray<uint> & types, IfCanConvert & owner ) override;
  virtual bool            IsNeedConvertToSavePrevious( SSArray<uint> & types, SaveDocumentVersion saveMode, IfCanConvert & owner ) override;

  /// �������� �� ��� ��������� �������� ������
  virtual bool            IsCanSetTolerance( const VarParameter & varParameter ) const override;
  /// ��� ��������� �������� ����������� ���������� ������
  virtual double          GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const override;
  /// �������� �� �������� �������
  virtual bool            ParameterIsAngular( const VarParameter & varParameter ) const override;

  // ���������� IfHotPoints
  /// ������� ���-�����
  virtual void GetHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool ) override;
  /// ����������� �������������� ���-�����
  virtual void UpdateHotPoints( RPArray<HotPointParam> & hotPoints,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool ) override;
  /// ������ �������������� ���-����� (�� ���-����� ������ ����� ������ ����)
  virtual bool BeginDragHotPoint( HotPointParam & hotPoint ) override;
  /// ���������� ��� �������������� ���-����� 
  // step - ��� ������������, ��� ����������� �������� � �������� ������������� �����
  virtual bool ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
    double step, D3Draw & pD3DrawTool ) override;
  /// ���������� �������������� ���-����� (�� ���-����� �������� ����� ������ ����)
  virtual bool EndDragHotPoint( HotPointParam & hotPoint, D3Draw & pD3DrawTool ) override;

  // ���������� ���-����� �������� ������ �� ShMtStraighten
  /// ������� ���-����� (��������������� ������� � 5000 �� 5999)
  virtual void GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ShMtBendObject &         children ) override; // #84586
                                                  /// ����������� �������������� ���-�����
  virtual void UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    ShMtBendObject &         children ) override;
  /// ���������� ��� �������������� ���-�����
  virtual bool ChangeChildrenHotPoint( HotPointParam &  hotPoint,
    const MbVector3D &     vector,
    double           step,
    ShMtBendObject & children ) override;

  /// ����� �� � ������� ���� ��������� ����������� �������
  virtual bool IsDimensionsAllowed() const override;
  virtual void GetHotPointsForDimensions( RPArray<HotPointParam> & hotPoints,
    IfComponent & editComponent,
    IfComponent & topComponent,
    bool allHotPoints = false ) override;
  /// ������������� ������� � ����������� ��������
  virtual void AssignDimensionsVariables( const SFDPArray<PartObject> & dimensions ) override;
  /// ������� ������� ������� �� ���-������
  virtual void CreateDimensionsByHotPoints( IfUniqueName * un,
    SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints ) override;
  /// �������� ��������� �������� ������� �� ���-������
  virtual void UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    const PArray<OperationSpecificDispositionVariant>* variants ) override;

  /// ������������� ������� �������� �������� - ������
  virtual void AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
    ShMtBendObject & children ) override;
  /// ������� ������� ������� �� ���-������ ��� �������� ��������
  virtual void CreateChildrenDimensionsByHotPoints( IfUniqueName * un,
    SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children ) override;
  /// �������� ��������� �������� ������� �� ���-������ ��� �������� ��������
  virtual void UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children ) override;

  /// ������ ���� ��� ��������� ������� ���������� �� �������
  virtual void UpdateMeasurePars( GeneralToleranceType gType, bool recalc ) override;


  /// ������ ������� ������
  virtual void SetRefObject( const WrapSomething * wrapper, size_t index ) override;

  /// �������� ������� ������
  virtual const WrapSomething & GetRefObject( size_t index ) const override;

  /// ��������� ������� ������ (��� �������)
  virtual const bool IsObjectSuitableAsBaseVertex( const WrapSomething & pointWrapper ) override;

  /// ��������� ������� ������ (��� ���������)
  virtual const bool IsObjectSuitableAsBasePlane( const WrapSomething & planeWrapper, bool forLeftSide ) override;

  /// ��������� �������� ����� ������� ����������� �����
  virtual void FixAbsSideLength( bool isLeftSide ) override;

protected:
  virtual const AbsReferenceContainer * GetRefContainer( const IfComponent & bodyOwner ) const override { return &m_bendObject; }

private:
  bool IsCanoutAdjoining() const; ///< ����� �� ������������ ��������� ����� ������ � � ������
  void DetermineTypeBens() const;
  /// �������� �������������� ������� ������������ ��������� �����
  MbeItemLocation BendObjectLocation( const WrapSomething & pointWrapper, bool left );
  bool IsNeedInvertDirectionBend();

  /// �������� ���������� ����� (KOMPAS-17824: �� ������ �������� ��� ����� � ����� �������� �����)
  bool VerifyNonOppositeEdge( const WrapSomething & bend, const MbCurveEdge & curveEdge ) const;

  /// ���������, ������� �� ������� ������� ��� ������������ ����� ������ �����
  bool IsValidBaseObjects();
  /// �������� ������ �������
  void ResetRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions, EDimensionsIds dimensionId );
  /// �������� ������ ����
  void ResetAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions );
  /// �������� ����� ���������� ���� �����
  double GetBendInternalLength( const HotPointBendOperUtil & utils ) const;
  /// �������� ����������� ����� ��� ������� �������� ���������� ��������
  HotPointBendOperUtil * GetHotPointUtil( MbBendByEdgeValues & params,
    IfComponent        & editComponent,
    ShMtBendObject     * children,
    bool ignoreStraightMode = false );
  /// �������� ����������� ����� ��� ������� �������� ���������� ��������
  HotPointBendOperUtil * GetHotPointUtil( MbBendByEdgeValues & params,
    IfComponent * editComponent,
    ShMtBendObject * children,
    bool ignoreStraightMode = false );

  /// �������� ������ ������� �����
  void UpdateRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    EDimensionsIds dimensionId,
    bool moveToExternal );
  /// �������� ������ ���� �����
  void UpdateAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils ) const;
  /// �������� ������ ����� �����
  void UpdateLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils ) const;
  /// �������� ������ ����� ����� �����/������
  void UpdateSideLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    bool isLeftSide ) const;
  /// �������� ������ �������� ������������ �������� ������� ��� ����������� ����� �����/������
  void UpdateSideObjectOffsetDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    bool isLeftSide ) const;
  /// �������� ������ �������� �����
  void UpdateDeepnessDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils ) const;
  /// �������� ������ ������� �����
  void UpdateIndentDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    const ShMtBendHotPoints hotPointId,
    const EDimensionsIds dimensionId,
    double indent ) const;
  /// �������� ������ ������ �����
  void UpdateWidthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints ) const;
  /// �������� ������ ���������� �����
  void UpdateWideningDimensions( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints );
  /// �������� ������� ������ ����������� �����
  void UpdateDeviationDimensions( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints ) const;
  /// �������� ������ ������������ ������
  void UpdateWidthReleaseDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints );
  /// �������� ������ ������������ - �������
  void UpdateDepthReleaseDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints );
  /// �������� ������ ���� ������ ������ �����
  void UpdateSideAngleDimensions( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints ) const;
  /// �������� ��������� ������� ����� �� �����������
  bool GetLengthDimensionParametersByContinue( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ����� �� �������
  bool GetLengthDimensionParametersByContour( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ����� �� �������
  bool GetLengthDimensionParametersByTouch( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ����� �� ������� ��� ���� ������ 90
  bool GetLengthDimensionObtuseAngle( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ����� �� ������� ��� ���� ������ 90 ������ �����
  void GetLengthDimensionObtuseAngleInternal( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ����� �� ������� ��� ���� ������ 90 �������
  void GetLengthDimensionObtuseAngleNotInternal( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ����, ��� �����������
  bool GetAngleDimensionSupplementary( const HotPointBendOperUtil & utils,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ����, ��� - ���� �����
  bool GetAngleDimensionBend( const HotPointBendOperUtil & utils,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ������ �����������
  bool GetDeviationDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendHotPoints deviationHotPointId,
    ShMtBendHotPoints sideAngleHotPointId,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ������������ ������
  bool GetWidthReleaseDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// �������� ��������� ������� ������������ �������
  bool GetDepthReleaseDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// �������� ��������� ������� ���� ������ ������� �����
  bool GetSideAngleDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    double dimensionLength,
    ShMtBendHotPoints hotPointId,
    double angle,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// �������� ��������� ������� ���������� �����
  bool GetWideningDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendHotPoints wideningHotPointId,
    double widening,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// �������� ��������� ������� ����������� ����� �����/������
  bool GetSideLengthDimensionParametersByContinue( const HotPointBendOperUtil & utils,
    MbPlacement3D        & dimensionPlacement,
    MbCartPoint3D        & dimensionPoint1,
    MbCartPoint3D        & dimensionPoint2,
    bool                   isLeftSide ) const;
  /// �������� ��������� ������� ����������� ����� �����/������
  bool GetSideLengthDimensionParametersByContour( const HotPointBendOperUtil & utils,
    MbPlacement3D        & dimensionPlacement,
    MbCartPoint3D        & dimensionPoint1,
    MbCartPoint3D        & dimensionPoint2,
    bool                   isInternal,
    bool                   isLeftSide ) const;
  /// �������� ��������� ������� ����������� ����� �����/������
  bool GetSideLengthDimensionParametersByTouch( const HotPointBendOperUtil & utils,
    MbPlacement3D        & dimensionPlacement,
    MbCartPoint3D        & dimensionPoint1,
    MbCartPoint3D        & dimensionPoint2,
    bool                   isInternal,
    bool                   isLeftSide ) const;
  /// �������� ��������� ������� �������� ������������ �������� ������� ��� ����������� ����� �����/������
  bool GetSideObjectOffsetDimensionParameters( const HotPointBendOperUtil & utils,
    MbPlacement3D        & dimensionPlacement,
    MbCartPoint3D        & dimensionPoint1,
    MbCartPoint3D        & dimensionPoint2,
    bool                   isLeftSide ) const;
  /// ��������� ����� ����������� ����� �� �������� �������
  const bool CalculateSideLengthToObject( HotPointBendOperUtil & utils,
    bool                   isLeftSide,
    double               & length );
  /// �������� ����������� � ���� � �����
  void GetArcTangent( const MbArc3D       & supportArc,
    const MbCartPoint3D & pointOnArc,
    MbLine3D      & tangent ) const;
  /// ����������� ����� �����, ���� �� ������ �� �������/�� ���������
  bool UpdateSideLengthsForNonAbsoluteCalcMode( MbBendByEdgeValues & params );

  bool GetIndentDimensionParameters( const HotPointBendOperUtil & utils, MbCartPoint3D * indentHotPointPosition, double indent,
    MbCartPoint3D & dimensionPoint1, MbCartPoint3D & dimensionPoint2, MbPlacement3D & dimensionPlacement ) const;

  bool GetDeepnessDimensionParameters( const HotPointBendOperUtil & utils, MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1, MbCartPoint3D & dimensionPoint2 ) const;

  bool GetRadiusDimensionParameters( const HotPointBendOperUtil & utils, bool moveToExternal,
    MbPlacement3D & dimensionPlacement, MbArc3D & dimensionArc );

  bool GetSideLengthDimensionParameters( const HotPointBendOperUtil & utils, bool isLeftSide,
    MbPlacement3D & dimensionPlacement, MbCartPoint3D & dimensionPoint1, MbCartPoint3D & dimensionPoint2 ) const;

  bool GetWidthDimensionParameters( const HotPointBendOperUtil & utils, MbCartPoint3D * widthHotPointPosition,
    MbCartPoint3D & dimensionPoint1, MbCartPoint3D & dimensionPoint2, MbPlacement3D & dimensionPlacement ) const;

private:
  void operator = ( const ShMtBend & ); // �� �����������

  DECLARE_PERSISTENT_CLASS( ShMtBend )
};


//--------------------------------------------------------------------------------
/**
�����������, ������ ���������� ��������� ������ �� �����������, ����� ����� �����.
����������� ��� ������������� � ������������ ��������.
\param _pars
\param un
\return
*/
//---
ShMtBend::ShMtBend( const ShMtBendParameters & _pars, IfUniqueName * un )
  : ShMtBaseBend( un ),
  m_params( _pars ),
  m_refObjects( 2 ),
  m_typeCircuitBend( 0 )
{
  ShMtBend::FlushBendObject();
  if ( un )
    m_params.RenameUnique( *un );
}


//--------------------------------------------------------------------------------
/**
����������� �����������
\param other
\return
*/
//---
ShMtBend::ShMtBend( const ShMtBend & other )
  : ShMtBaseBend( other ),
  m_params( other.m_params ),
  m_refObjects( other.m_refObjects ),
  m_typeCircuitBend( other.m_typeCircuitBend )
{
}


//--------------------------------------------------------------------------------
/**
�����������
\param
\return
*/
//---
ShMtBend::ShMtBend( TapeInit )
  : ShMtBaseBend( tapeInit ),
  m_params( nullptr ),
  m_refObjects( 2 ),
  m_typeCircuitBend( 0 )
{
  ShMtBend::FlushBendObject();
}


I_IMP_SOMETHING_FUNCS_AR( ShMtBend )


//--------------------------------------------------------------------------------
/**
��������� ����������
*/
//---
I_IMP_QUERY_INTERFACE( ShMtBend )
I_IMP_CASE_RI( ShMtBend );
I_IMP_CASE_RI( HotPoints );
I_IMP_CASE_RI( CanConvert );
I_IMP_CASE_RI( ShMtCircuitBend );
I_IMP_END_QI_FROM_BASE2( ShMtBaseBend, RefContainersOwner )


//--------------------------------------------------------------------------------
/**
������� ����� ������� - ����� ��������� ����������� ������
\param compOwner
\return IfPartObjectCopy *
*/
//---
IfPartObjectCopy * ShMtBend::CreateObjectCopy( IfComponent & compOwner ) const
{
  return ::CreateObjectCopy<PartObject, PartObjectCopy>( compOwner, *this );
}


//--------------------------------------------------------------------------------
/**
������� ���������� �� ��������� ����������
\param component
\return bool
*/
//---
bool ShMtBend::NeedCopyOnComp( const IfComponent & component ) const
{
  return false;
}


//--------------------------------------------------------------------------------
/**
����� �� ���������� ��� �������� ���-������ �����, ����� ��� �������������?
�.�. ����� MakePureSolid ��� ����� ���������� IfCopibleOperationOnPlaces,
IfCopibleOperationOnEdges � �.�.
\return bool
*/
//---
bool ShMtBend::CanCopyNonGeometrically() const
{
  return true;
}


//--------------------------------------------------------------------------------
/**
������ ������ �� �������, ������ ������� ����� ����� �� ��������� ����������
\param comp
\return const AbsReferenceContainer *
*/
//---
const AbsReferenceContainer * ShMtBend::GetReferencesToFind( const IfComponent & comp )
{
  return &m_bendObject;
}


//--------------------------------------------------------------------------------
/**
������������ �����
\param result
\param allRefs
\param withMath
\return bool
*/
//---
bool ShMtBend::RestoreReferences( PartRebuildResult & result, bool allRefs, CERst withMath )
{
  bool res = ShMtBaseBend::RestoreReferences( result, allRefs, withMath );

  if ( IsJustRead() && GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0B000001L )
  {
    // ��� ������ ������ 11 ���� ���������� �� ����� � ��������
    IFPTR( Primitive ) prim( m_bendObject.GetObjectTrans( 0 ).obj );
    if ( prim )
      for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
      {
        m_bendObjects[i]->m_extraName = prim->GetShellThreadId();
      }
  }

  bool vertexRefRes = m_refObjects.RestoreReferences( result, allRefs, withMath );

  return res && vertexRefRes;
}


//--------------------------------------------------------------------------------
/**
�������� ������� ������� soft = true - ������ ������� (�� ������ �����)
\param soft
\return void
*/
//---
void ShMtBend::ClearReferencedObjects( bool soft )
{
  ShMtBaseBend::ClearReferencedObjects( soft );
  m_refObjects.ClearObjects( soft );
}


//--------------------------------------------------------------------------------
/**
�������� ������ �� �������
\param changedRefs
\return bool
*/
//---
bool ShMtBend::ChangeReferences( ChangeReferencesData & changedRefs )
{
  bool bendRef = ShMtBaseBend::ChangeReferences( changedRefs );
  bool vertexRef = m_refObjects.ChangeReferences( changedRefs );
  return bendRef || vertexRef;
}


//--------------------------------------------------------------------------------
/**
������������� �������
\param operOwner
\param setNames
\return bool
*/
//---
bool ShMtBend::InitPhantom( const IfComponent &operOwner, bool setNames )
{
  return InitPhantomPrevSolid( operOwner, tct_Created | tct_Merged | tct_Truncated/*changed*/, true /*false*/, true );  // KVT ��� ������� ������ ���� ������������
}


//--------------------------------------------------------------------------------
/**
���� ���������� ������, ������� ��� ����������� ������ ���� �������� �� �����
\param internalRefs
\return bool
*/
//---
bool ShMtBend::GetInternalReferences( AbsReferenceContainer & internalRefs )
{
  return false;
}


//--------------------------------------------------------------------------------
/**
��������� �������� ��� ��������� �����.
\param mksdParam
\param copyPlace
\param foundEdges
\param internalEdgesPairs
\param mainId
\param needInvert
\param resultSolid
\param posData
\return uint
*/
//---
uint ShMtBend::MakeSolidOnEdges( MakeSolidParam        & mksdParam,
  const MbPlacement3D   & copyPlace,
  FDPArray<MbCurveEdge> & foundEdges,
  EdgesPairs        *     internalEdgesPairs,
  SimpleName              mainId,
  bool                    needInvert,
  MbSolid              *& resultSolid,
  PArray<MbPositionData> * posData )
{
  uint res = rt_Success;
  MbSolid * result = nullptr; // ���������

  std::vector<MbCurveEdge*> edges;
  for ( size_t i = 0, count = m_bendObject.Count(); i < count; ++i )
  {
    IfSomething * obj = m_bendObject.GetObjectTrans( i ).obj;
    IFPTR( PartEdge ) partEdge( obj );
    // ���� �����, � ��� �� ������� �������, ���� ���� �������� �� ������� ��� ���������
    if ( !!partEdge && IsValidBaseObjects() )
      if ( MbCurveEdge * curveEdge = partEdge->GetMathEdge() )
        edges.push_back( curveEdge );
  }


  // ������� ���� �� ��������� �����
  if ( edges.size() )
  {
    result = PrepareLocalSolid( edges, res, mksdParam, -1, mainId );
  }
  else
    res = rt_NoEdges; // ������


  if ( result )
    resultSolid = result;

  return res;
}


uint ShMtBend::GetObjType()  const { return ot_ShMtBend; } // ������� ��� �������
uint ShMtBend::GetObjClass() const { return oc_ShMtBend; } // ������� ����� ��� �������
uint ShMtBend::OperationSubType() const { return bo_Union; } // ������� ������ ��������


                                                             //--------------------------------------------------------------------------------
                                                             /**
                                                             ����������� ������
                                                             \return PrObject &
                                                             */
                                                             //---
PrObject & ShMtBend::Duplicate() const
{
  return *new ShMtBend( *this );
}


//--------------------------------------------------------------------------------
/**
���������� ��������� �� ������� �������
\param other
\return void
*/
//---
void ShMtBend::Assign( const PrObject &other )
{
  ShMtBaseBend::Assign( other );

  if ( const ShMtBend * oper = dynamic_cast<const ShMtBend*>(&other) )
  {
    m_refObjects.Init( oper->m_refObjects );
    m_params = oper->m_params;
    m_typeCircuitBend = oper->m_typeCircuitBend;
  }

  UpdateMeasurePars( gtt_none, true );
}


//--------------------------------------------------------------------------------
/**
������ ���������
\param p
\return void
*/
//---
void ShMtBend::SetParameters( const ShMtBaseBendParameters & p )
{
  m_params = (ShMtBendParameters&)p;
}


//--------------------------------------------------------------------------------
/**
�������� ���������
\return ShMtBaseBendParameters &
*/
//---
ShMtBaseBendParameters & ShMtBend::GetParameters() const
{
  return const_cast<ShMtBendParameters&>(m_params);
}


//--------------------------------------------------------------------------------
/**
������ ������ �����
\param wrapper
\return void
*/
//---
void ShMtBend::SetBendObject( const WrapSomething * wrapper )
{
  if ( wrapper && *wrapper )
  {
    if ( GetBendCount() )
      ShMtBaseBend::SetObject( wrapper );
    else
      AddBendObject( *wrapper );
  }
  else
    AddBendObject( GetBend( 0 ) );

  //ShMtBaseBend::SetObject( wrapper );
}


//--------------------------------------------------------------------------------
/**
�������� ������ �����
\return const WrapSomething &
*/
//---
const WrapSomething & ShMtBend::GetBendObject() const
{
  static WrapSomething emptyBend;
  emptyBend.Clear();

  return GetBendCount() ? ShMtBaseBend::GetObject() : emptyBend;
}


#ifdef MANY_THICKNESS_BODY_SHMT
//--------------------------------------------------------------------------------
/**
������� �� ��������� ������
\param wrapper
\return bool
*/
//---
bool ShMtBend::IsSuitedObject( const WrapSomething & wrapper )
{
  bool res = false;

  // �� K11 17.12.2008 ������ �37142: ��������� ����� ����� ������������ ���
  if ( wrapper && wrapper.component == wrapper.editComponent &&
    !::IsMultiShellBody( wrapper ) )
  {
    CIFPTR( PartEdge ) pEdge( wrapper.obj );
    if ( pEdge )
    {
      if ( MbCurveEdge * curveEdge = pEdge->GetMathEdge() )
      {
        double thickness;
        if ( CalcShMtThickness( nullptr, *curveEdge, thickness ) )
          res = IsSuitedObject( *curveEdge, thickness );

        if ( res )
        {
          CIFPTR( Primitive ) pPrimitiveNew( wrapper.obj );
          if ( pPrimitiveNew )
          {
            IFPTR( Primitive ) pPrimitiveOld;
            DetailSolid * pDetailSolidOld = nullptr, *pDetailSolidNew = nullptr;
            WrapSomething bend;

            for ( size_t i = 0, count = GetBendCount(); i < count && res; ++i )
            {
              if ( bend = GetBend( i ) )
              {
                if ( pPrimitiveOld = IFPTR( Primitive )(bend.obj) )
                {
                  pDetailSolidOld = bend.component->ReturnDetailSolid( true );
                  pDetailSolidNew = wrapper.component->ReturnDetailSolid( true );
                  if ( pDetailSolidOld && pDetailSolidNew && pDetailSolidOld == pDetailSolidNew )
                  {
                    res = pDetailSolidOld->GetFinalId( pPrimitiveOld->GetShellThreadId() ) == pDetailSolidNew->GetFinalId( pPrimitiveNew->GetShellThreadId() );

                    if ( res )
                      res = VerifyNonOppositeEdge( bend, *curveEdge );
                  }
                }
              }
            }   // for
          }
        }
      }
    }
  }

  return res;
}


#else
//--------------------------------------------------------------------------------
/**
������� �� ��������� ������
\param wrapper
\param thickness
\return bool
*/
//---
bool ShMtBend::IsSuitedObject( const WrapSomething & wrapper, double thickness )
{
  bool res = false;

  // �� K11 17.12.2008 ������ �37142: ��������� ����� ����� ������������ ���
  if ( !::IsMultiShellBody( wrapper ) && wrapper.obj )
  {
    IFPTR( PartEdge ) pEdge( wrapper.obj );

    if ( pEdge )
    {
      MbCurveEdge * curveEdge = pEdge->GetMathEdge();

      if ( curveEdge )
      {
        res = IsSuitedObject( *curveEdge, thickness );

        if ( res )
        {
          WrapSomething bend;
          for ( size_t i = 0, count = GetBendCount(); i < count && res; ++i )
          {
            if ( bend = GetBend( i ) )
              res = VerifyNonOppositeEdge( bend, *curveEdge );
          }
        }
      }
    }
  }

  return res;
}
#endif


//--------------------------------------------------------------------------------
/**
�������� ���������� ����� (KOMPAS-17824: �� ������ �������� ��� ����� � ����� �������� �����)
\param bend
\param curveEdge
\return bool
*/
//---
bool ShMtBend::VerifyNonOppositeEdge( const WrapSomething & bend, const MbCurveEdge & curveEdge ) const
{
  bool res = true;

  CIFPTR( PartEdge ) oldEdge( bend.obj );
  if ( oldEdge )
  {
    MbFace * newSheetFace = nullptr, *newOppositeFace = nullptr, *newFacePlus = nullptr, *newFaceMinus = nullptr,
      *oldSheetFace = nullptr, *oldFacePlus = nullptr, *oldFaceMinus = nullptr;
    MbCurveEdge * oldCurveEdge = nullptr;

    if ( !newFacePlus ) newFacePlus = curveEdge.GetFacePlus();
    if ( !newFaceMinus ) newFaceMinus = curveEdge.GetFaceMinus();
    if ( !newSheetFace ) newSheetFace = FindSheetFace( curveEdge );
    if ( newSheetFace && !newOppositeFace ) newOppositeFace = FindPairBendFace( *newSheetFace );

    oldCurveEdge = oldEdge->GetMathEdge();

    oldFacePlus = oldCurveEdge->GetFacePlus();
    oldFaceMinus = oldCurveEdge->GetFaceMinus();  // KOMPAS-17824: �� �������� ����� �����,
    oldSheetFace = FindSheetFace( *oldCurveEdge );// �������� ����� ����� � �����-���� ����� ��������� ������
                                                  // � �������� �� ��������������� �� ���� ����� ��������� ����
    res = !(oldFacePlus && oldFaceMinus && oldSheetFace && newOppositeFace && newFacePlus && newFaceMinus
      && (newFacePlus == oldFacePlus || newFacePlus == oldFaceMinus || newFaceMinus == oldFacePlus || newFaceMinus == oldFaceMinus)
      && (oldSheetFace == newOppositeFace));
  }

  return res;
}


//--------------------------------------------------------------------------------
/**
TODO: ��������
\return double
*/
//---
double ShMtBend::GetThickness() const
{
  return m_params.m_thickness;    // �������
}


//--------------------------------------------------------------------------------
/**
TODO: ��������
\param h
\return void
*/
//---
void ShMtBend::SetThickness( double h )
{
  m_params.m_thickness.ForgetVariable();
  m_params.m_thickness = h;    // �������
}


//--------------------------------------------------------------------------------
/**
TODO: ��������
\return double
*/
//---
double ShMtBend::GetOwnerAbsRadius() const
{
  //  return m_params.GetRadius();
  return m_params.m_internal ? m_params.GetRadius() : (m_params.GetRadius() - GetThickness());    // ���������� ������ �����
}


//--------------------------------------------------------------------------------
/**
�����������, ������������ ��������� ������������ ����
\return double
*/
//---
double ShMtBend::GetOwnerValueBend() const
{
  double val;
  m_params.GetValueBend( val );
  return val;
}


//--------------------------------------------------------------------------------
/**
TODO: ��������
\return double
*/
//---
double ShMtBend::GetOwnerAbsAngle() const
{
  return  m_params.GetAbsAngle();
}


//--------------------------------------------------------------------------------
/**
�� ����������� �������
\return bool
*/
//---
bool ShMtBend::IsOwnerInternalRad() const {
  return m_params.m_internal;
}


//--------------------------------------------------------------------------------
/**
���� ��� ���������
\return UnfoldType
*/
//---
UnfoldType ShMtBend::GetOwnerTypeUnfold() const {
  return m_params.m_typeUnfold;
}


//--------------------------------------------------------------------------------
/**
������ ��� ���������
\param t
\return void
*/
//---
void ShMtBend::SetOwnerTypeUnfold( UnfoldType t )
{
  m_params.m_typeUnfold = t;
}


//--------------------------------------------------------------------------------
/**
TODO: ��������
\param val
\return void
*/
//---
void ShMtBend::SetOwnerFlagByBase( bool val )
{
  m_params.SetFlagUnfoldByBase( val );
}


//--------------------------------------------------------------------------------
/**
TODO: ��������
\return bool
*/
//---
bool ShMtBend::GetOwnerFlagByBase() const
{
  return m_params.GetFlagUnfoldByBase();
}


//--------------------------------------------------------------------------------
/**
���� ���������� ����� ���� ���������, � ���� ������ ���� �� ����, �� ��������� ����
\param trans
\return bool
*/
//---
bool ShMtBend::IfOwnerBaseChanged( IfComponent * trans )
{
  return m_params.ReadBaseIfChanged( trans );
}


//--------------------------------------------------------------------------------
/**
�������� �����������
\return double
*/
//---
double ShMtBend::GetOwnerCoefficient() const
{
  return m_params.GetCoefficient();
}


//--------------------------------------------------------------------------------
/**
���������� ��� ��������� ���� ��������� �����.
*/
//---
void ShMtBend::DetermineTypeBens() const
{
  PArray<MbCurveEdge> curveEdges( 0, 1, false );

  for ( size_t i = 0, c = GetBendCount(); i < c; i++ )
  {
    WrapSomething edge = GetBend( i );
    IFPTR( PartEdge ) pEdge( edge.obj );
    if ( MbCurveEdge * curveEdge = pEdge ? pEdge->GetMathEdge() : nullptr )
      curveEdges.Add( curveEdge );
  }

  m_typeCircuitBend = 0;

  if ( ::IsAdjacentEdges( curveEdges ) )
    m_typeCircuitBend = m_typeCircuitBend | TypeCircuitBend::tcb_AdjoiningBetween;

  if ( curveEdges.Count() > 1 && ::IsOpenLoopEdges( curveEdges ) )
    m_typeCircuitBend = m_typeCircuitBend | TypeCircuitBend::tcb_AdjoiningBeginEnd;
}


//--------------------------------------------------------------------------------
/**
��������� �� ����.
*/
//---
bool ShMtBend::IsClosedPath() const
{
  return !IsCanoutAdjoining();
}


//--------------------------------------------------------------------------------
/**
����� �� ������������ ��������� ������� �����.
*/
//---
bool ShMtBend::IsCanoutAdjoiningBetween() const
{
  return !!(m_typeCircuitBend & TypeCircuitBend::tcb_AdjoiningBetween);
}


//------------------------------------------------------------------------------
/**
����� �� ������������ ��������� ����� ������ � � ������.
*/
//---
bool ShMtBend::IsCanoutAdjoining() const
{
  return !!(m_typeCircuitBend & TypeCircuitBend::tcb_AdjoiningBeginEnd);
}


//--------------------------------------------------------------------------------
/**
����� �� ������������ ��������� ����� �������.
*/
//---
bool ShMtBend::IsCanoutAdjoiningBegin()
{
  return IsCanoutAdjoining();
}


//--------------------------------------------------------------------------------
/**
����� �� ������������ ��������� ����� ������.
*/
//---
bool ShMtBend::IsCanoutAdjoiningEnd()
{
  return IsCanoutAdjoining();
}


//--------------------------------------------------------------------------------
/**
����� ��� ��� ����������� ��������������
\param iniObj
\param editComp
\return void
*/
//---
void ShMtBend::EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp )
{
  IFPTR( ShMtBend ) ini( const_cast<IfSheetMetalOperation*>(&iniObj) );
  if ( ini )
    m_params.EditingEnd( (ShMtBendParameters&)ini->GetParameters(), *editComp.ReturnPartRebuildResult(), this );
}


IMP_PERSISTENT_CLASS( kompasDeprecatedAppID, ShMtBend );
IMP_PROC_REGISTRATION( ShMtBend );


//--------------------------------------------------------------------------------
/**
�������� ���������� ��� ��������������� ����
\param codeError
\param mksdParam
\param shellThreadId
\return MbSolid *
*/
//---
MbSolid * ShMtBend::MakeSolid( uint              & codeError,
  MakeSolidParam    & mksdParam,
  uint                shellThreadId )
{
  uint res = rt_Success;
  MbSolid * result = nullptr;

  if ( mksdParam.m_prevSolid && mksdParam.m_prevSolid->IsMultiSolid() )
    res = rt_MultiSolidDeflected;

  if ( res == rt_Success )
  {
    res = rt_Error;
    std::vector<MbCurveEdge*> edges;
    for ( size_t i = 0, count = m_bendObject.Count(); i < count; ++i )
    {
      IfSomething * obj = m_bendObject.GetObjectTrans( i ).obj;
      IFPTR( PartEdge ) partEdge( obj );
      // ���� �����, � ��� �� ������� �������, ���� ���� �������� �� ������� ��� ���������
      if ( !!partEdge && IsValidBaseObjects() )
        if ( MbCurveEdge * curveEdge = partEdge->GetMathEdge() )
          edges.push_back( curveEdge );
    }

    DetermineTypeBens();

    if ( edges.size() )
    {
      result = PrepareLocalSolid( edges, res, mksdParam, shellThreadId, MainId() );
    }
  }

  codeError = res;
  return result;


  //   uint res = rt_Success;
  //   MbSolid * result = nullptr;
  // 
  //   if ( mksdParam.m_prevSolid && mksdParam.m_prevSolid->IsMultiSolid() )
  //     res = rt_MultiSolidDeflected;
  //   
  //   if ( res == rt_Success )
  //   {
  //     res = rt_Error;
  // 
  //     uint resLocal = rt_Success;
  //     for ( size_t i = 0, count = m_bendObject.Count(); i < count && resLocal == rt_Success; ++i )
  //     {
  //       IfSomething * obj = m_bendObject.GetObjectTrans(i).obj;
  //       IFPTR( PartEdge ) partEdge ( obj );
  //       // ���� �����, � ��� �� ������� �������, ���� ���� �������� �� ������� ��� ���������
  //       if ( !!partEdge && IsValidBaseObjects() )
  //       {
  //         MbCurveEdge * curveEdge = partEdge->GetMathEdge();
  //         if ( curveEdge )
  //         {
  //           MbSolid * oldResult = result;;
  //           result = PrepareLocalSolid( *curveEdge, resLocal, mksdParam, shellThreadId, MainId(), oldResult ? oldResult : mksdParam.m_prevSolid );
  //           if ( result == nullptr )
  //             result = oldResult;
  // 
  //           if ( oldResult && result && oldResult != result )
  //             ::DeleteItem( oldResult );             
  //         }
  //       }
  //     }
  //     res = resLocal;
  //   }
  //   
  //   codeError = res;
  //   return result;
}


//------------------------------------------------------------------------------
/**
����������(�������� ���� ����� ��� ����) ����� ��� ���������� �����.
\return bool ��������� ��� ��������.
*/
//---
bool ShMtBend::AddBendObject( const WrapSomething & bend )
{
  bool result = bend && !m_bendObject.IsObjectExist( bend );

  if ( result )
    m_bendObject.AddObject( bend );
  else
    RemoveBendObject( m_bendObject.FindObject( bend ) );
  //m_bendObject.DeleteObject( bend );

  return result;
}


//------------------------------------------------------------------------------
/**
�������� ����� �� �������.
\return bool ������� ������� ��� ������ ������� ���.
*/
//---
bool ShMtBend::RemoveBendObject( size_t index )
{
  bool result = index < m_bendObject.Count();

  if ( result )
    m_bendObject.DeleteInd( index );

  if ( index < m_bendObjects.Count() )
    m_bendObjects.RemoveInd( index );

  return result;
}



//------------------------------------------------------------------------------
/**
�������� ��� ����� �����.
*/
//---
void ShMtBend::FlushBendObject()
{
  m_bendObject.Reset();
}


//------------------------------------------------------------------------------
/**
�������� ����� ��� ���������� ����� �� �������
*/
//---
WrapSomething ShMtBend::GetBend( size_t i ) const
{
  return m_bendObject.GetObjectTrans( i );
}


//------------------------------------------------------------------------------
/**
�������� ���������� ����� ��� ���������� �����.
\return size_t ���������� ����� ����������� � ���������� �����.
*/
//---
size_t ShMtBend::GetBendCount() const
{
  return m_bendObject.Count();
}


//------------------------------------------------------------------------------
/**
�������� �� ���� �� ���������� ������.
*/
//---
bool ShMtBend::IsMultiBend() const
{
  return m_bendObject.Count() > 1;
}


//--------------------------------------------------------------------------------
/**
�������� ����������� ��������
\return bool
*/
//---
bool ShMtBend::IsValid() const
{
  return ShMtBaseBend::IsValid() && m_params.IsValidParam() && m_bendObject.IsFull();
}


//--------------------------------------------------------------------------------
/**
�������� ������ �������������� �� �������
\param warnings
\return void
*/
//---
void ShMtBend::WhatsWrong( WarningsArray & warnings )
{
  ShMtBaseBend::WhatsWrong( warnings );

  if ( !m_bendObject.IsFull() )
  {
    warnings.AddWarning( IDP_WARNING_LOSE_SUPPORT_OBJ, module ); // "������� ���� ��� ��������� ������� ��������"
  }

  IFPTR( PartEdge ) partEdge( GetBendObject().obj );
  if ( partEdge ) {
    MbCurveEdge * curveEdge = partEdge->GetMathEdge();
    if ( curveEdge ) {
      MbBendByEdgeValues params;
      m_params.GetValues( params, curveEdge->GetMetricLength(), GetObjVersion() );

      UtilBendError errors( params, GetThickness(), m_params, *curveEdge );
      errors.WhatsWrong( warnings );
    }
  }

  if ( !IsValidBaseObjects() )
    warnings.AddWarning( IDP_WARNING_LOSE_SUPPORT_OBJ, module );

  // �������� �� ���������� �������� ��������� ����� ����������� ����� (��� ������� �� ������� � ����� ���)
  if ( (m_params.m_sideLeft.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute &&
    m_params.m_sideLeft.m_length.GetVarValue() < 0) ||
    (m_params.m_bendLengthBy2Sides && m_params.m_sideRight.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute &&
      m_params.m_sideRight.m_length.GetVarValue() < 0) )
    warnings.AddWarning( IDP_RT_PARAMETERERROR, module );

  m_params.WhatsWrongParam( warnings );
}


//--------------------------------------------------------------------------------
/**
������������� �� ����������� � ������
\param result
\param toObj
\return bool
*/
//---
bool ShMtBend::PrepareToAdding( PartRebuildResult & result, PartObject * toObj )
{
  if ( ShMtBaseBend::PrepareToAdding( result, toObj ) )
  {
    if ( !GetFlagValue( O3D_IsReading ) ) // �� �������� �� ������, � ������ ��� new  BUG 68896
    {
      m_params.PrepareToAdding( result, this );
      UpdateMeasurePars( gtt_none, true );
    }
    return true;
  }

  return false;
}


//--------------------------------------------------------------------------------
/**
������ ������ ����������, ��������� � �����������
\param parVars
\param parCollType
\return void
*/
//---
void ShMtBend::GetVariableParameters( VarParArray & parVars, ParCollType parCollType )
{
  ::ShMtSetAsInformation( m_params.m_radius, ShMtManager::sm_radius, m_bendObjects.Count() > 0 && !IsAnySubBendByOwner() );

  ShMtBaseBend::GetVariableParameters( parVars, parCollType );

  bool addDivisionParam = IsCanoutAdjoiningBegin() && IsCanoutAdjoiningEnd() && !IsClosedPath();
  m_params.GetVariableParameters( parVars, parCollType == all_pars, true, addDivisionParam, IsMultiBend(), GetCircuitParameters().IsAnyCreateParam() );
}


//--------------------------------------------------------------------------------
/**
������ ��� ����������, ��������� � �����������
\return void
*/
//---
void ShMtBend::ForgetVariables()
{
  ShMtBaseBend::ForgetVariables();
  m_params.ForgetVariables();
}


//--------------------------------------------------------------------------------
/**
�������� ���������� ��� ���������
\param un
\return void
*/
//---
void ShMtBend::RenameUnique( IfUniqueName & un )
{
  ShMtBaseBend::RenameUnique( un );
  m_params.RenameUnique( un );
}


//--------------------------------------------------------------------------------
/**
������������� � ����������
\param operOwner
\return void
*/
//---
void ShMtBend::PrepareToBuild( const IfComponent & operOwner ) // ��������� - �������� ���� ��������
{
  creatorBends = new CreatorForBendUnfoldParams( *this, (IfComponent*)&operOwner );
}


//--------------------------------------------------------------------------------
/**
�������� ����� ������������
\return void
*/
//---
void ShMtBend::PostBuild()
{
  delete creatorBends;
  creatorBends = nullptr;
}


//--------------------------------------------------------------------------------
/**
����������� �������� �������
\param source
\return void
*/
//---
bool ShMtBend::CopyInnerProps( const IfPartObject& source,
  uint sourceSubObjectIndex,
  uint destSubObjectIndex )
{
  bool result = ShMtBaseBend::CopyStyleInnerProps( source );

  if ( const ShMtBend* pSource = dynamic_cast<const ShMtBend*>(&source) )
  {
    ShMtBendParameters::BendDisposal bendDisposal = m_params.m_bendDisposal;
    m_params = pSource->m_params;
    m_params.m_bendDisposal = bendDisposal;
  }

  return result;
}


//--------------------------------------------------------------------------------
/**
���������� ���������� ��� ��������������� ����
\param curveEdge
\param codeError
\param mksdParam
\param shellThreadId
\param mainId
\return MbSolid *
*/
//---
MbSolid * ShMtBend::PrepareLocalSolid( std::vector<MbCurveEdge*> edges,
  uint &        codeError,
  MakeSolidParam  & mksdParam,
  uint          shellThreadId,
  SimpleName    mainId
)
{
  MbSolid     *  result = nullptr;

  PartRebuildResult * partRResult = mksdParam.m_operOwner ? mksdParam.m_operOwner->ReturnPartRebuildResult() : nullptr;
  if ( mksdParam.m_prevSolid && partRResult && edges.size() && edges[0] )
  {
    double thickness = 0.0;
    if ( CalcShMtThickness( nullptr, *edges[0], thickness ) &&
      IsSuitedObject( *edges[0], thickness ) )
    {
      MbPlacement3D place;
      MbSNameMaker pComplexName( mainId, MbSNameMaker::i_SideNone, edges[0]->GetMainName() );
      pComplexName.SetVersion( GetObjVersion() );

      //MbFace * fPlus = edges[0]->GetFacePlus();
      //MbFace * facePlusCopy = fPlus->GetName().IsSheet() ? fPlus : edges[0]->GetFaceMinus();

      MbBendByEdgeValues params;
      SetThickness( thickness );

      m_params.ReadBaseIfChanged( const_cast<IfComponent*>(mksdParam.m_operOwner) ); // ���� ��������� ��������� ����� �� ����, �� ���������� ����, ���� ����
      m_params.GetValues( params, edges[0]->GetMetricLength(), GetObjVersion(), edges.size() > 1 ? ShMtBendParameters::bd_allLength : m_params.m_bendDisposal );
      // � ������ ������� ����������� �������������� ��������
      if ( GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0800000BL )
        // �������� ��������� ����� ���� ����������
        m_params.CalcAfterValues( params );

      SArray<uint> errors;

      for ( MbCurveEdge * edge : edges )
      {
        UtilBendError bennErrors( params, GetThickness(), m_params, *edge );
        bennErrors.GetErrors( errors );
      }

      // ������, ������� ����� ���������������� �� ���� ������
      if ( errors.Count() == 0 )
      {
        // ���� ���� �� ���� �� ���� ��������� �� � ���������� ����, �� �� ���� �����������
        if ( IsNeedInvertDirectionBend() )
          m_params.m_dirAngle = !m_params.m_dirAngle;

        if ( creatorBends )
        {
          creatorBends->Build( edges.size()/*count*/, shellThreadId );
          creatorBends->m_prepare = mksdParam.m_wDone != wd_Phantom;
          if ( !m_params.m_dirAngle )
            creatorBends->InvertAngleDirection();
          creatorBends->GetParams( params, true );
        }
        // � ������ ������� ����������� �������������� ��������
        if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
          // �������� ��������� ����� ���� ����������
          m_params.CalcAfterValues( params );

        // ���� ���� �� ���� �� ���� ��������� �� � ���������� ����, �� �� ���� �����������
        bool suitableParamValues = true;
        if ( m_params.m_sideLeft.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute ||
          (m_params.m_sideRight.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute && m_params.m_bendLengthBy2Sides) )
          suitableParamValues = UpdateSideLengthsForNonAbsoluteCalcMode( params );

        if ( IsValidParamBaseTable() && suitableParamValues )
        {
          RPArray<MbCurveEdge> curveEdges( 1, INDEX_TO_UINT( edges.size() ) );
          for ( MbCurveEdge * edge : edges )
            curveEdges.Add( edge );

          codeError = BendSheetSolidByEdges( *mksdParam.m_prevSolid/*mksdParam.m_prevSolid*/,     // �������� ����
            mksdParam.m_wDone == wd_Phantom ? cm_Copy : cm_KeepHistory,  // �������� � ������ �������� ��������� ����
            curveEdges,                // ���� ������
            IsStraighten(),            // �����
            params,                    // ��������� ����� 
            pComplexName,              // �����������
            creatorBends->m_mbPars,    // ��������� ������� �� ���������, ����� ���� ���������� �������� ���������� � ������� ��������� �����
            result );                  // �������������� ����

          if ( mksdParam.m_wDone != wd_Phantom )
            // ���� ��������� ��� ����� � �� �������
            creatorBends->SetMultiResult( result );

          if ( mksdParam.m_wDone != wd_Phantom && codeError == rt_SelfIntersect )
            codeError = rt_Success;

          if ( mksdParam.m_wDone != wd_Phantom && codeError == rt_Success && result )
            if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x07000110L )
              ::CheckIndexCutedBendFaces( *result, MainId() ); // ��� ���������� ������������� ������ �����
        }
      }


      if ( codeError != rt_Success && codeError != rt_SelfIntersect && result )
      {
        ::DeleteItem( result );
        result = nullptr;
      }
    } // if IsSuitedObject
    else
      codeError = rt_Error;
  } // if partRResult

  return result;
}


//--------------------------------------------------------------------------------
/**
������� �� ��������� ������ ��� �����
\param curveEdge
\param thickness
\return bool
*/
//---
bool ShMtBend::IsSuitedObject( const MbCurveEdge & curveEdge, double thickness )
{
  bool res = false;

  MbFace *shMtFace = nullptr;
  MbFace *facePlus = curveEdge.GetFacePlus();
  MbFace *faceMinus = curveEdge.GetFaceMinus();
  if ( facePlus && faceMinus ) {
    double angle;
    // ���� ����� ������� ������ ���� 90
    if ( facePlus->AngleWithFace( *faceMinus, angle ) && ::fabs( M_PI_2 - angle ) < Math::metricEpsilon )
    {
      shMtFace = ::FindSheetFace( curveEdge );
      if ( shMtFace != nullptr )
        res = true;
    }
  }

  return res;
}


//--------------------------------------------------------------------------------
/**
�������� �� ��� ��������� �������� ������
\param varParameter
\return bool
*/
//---
bool ShMtBend::IsCanSetTolerance( const VarParameter & varParameter ) const
{
  if ( m_params.IsContainsVarParameter( varParameter ) )
    return m_params.IsCanSetTolerance( varParameter );
  else
  {
    if ( IsSubBendsParameter( varParameter ) )
      return IsCanSetToleranceOnSubBendsParameter( varParameter );
    else
      return ShMtBaseBend::IsCanSetTolerance( varParameter );
  }
}


//------------------------------------------------------------------------------
// �������� �� �������� �������
// ---
bool ShMtBend::ParameterIsAngular( const VarParameter & varParameter ) const
{
  if ( &varParameter == &const_cast<ShMtBendParameters&>(m_params).GetAngleVar() ||
    &varParameter == &m_params.m_sideLeft.m_deviation || // ���� ������ ����������� �����
    &varParameter == &m_params.m_sideRight.m_deviation ||
    &varParameter == &m_params.m_sideLeft.m_angle || // ���� ������ ���� ����� 
    &varParameter == &m_params.m_sideRight.m_angle )
    return  true;

  return false;
}


//--------------------------------------------------------------------------------
/**
��� ��������� �������� ����������� ���������� ������
\param varParameter
\param tType
\param reader
\return double
*/
//---
double ShMtBend::GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const
{
  if ( &varParameter == &const_cast<ShMtBendParameters&>(m_params).GetAngleVar() )
  {
    //    double angle = m_params.m_typeAngle ? m_params.GetAngle() : 180 - m_params.GetAngle();
    // CC K14 ��� ��������� ������� ���������� ������. ����������� � ����������
    return reader.GetTolerance( m_params.m_distance, // ������� ������ BUG 62631
      vd_angle,
      tType );
  }
  else if ( &varParameter == &m_params.m_sideLeft.m_deviation || // ���� ������ ����������� �����
    &varParameter == &m_params.m_sideRight.m_deviation )
  {
    if ( m_params.m_sideLeft.m_length < Math::paramEpsilon ||
      ::fabs( varParameter.GetDoubleValue() ) < Math::paramEpsilon )
      return 0.0;
    // VB � ����� ��������� ��������� ���������, ��� �� ����� ������ ����
    return reader.GetTolerance( m_params.m_sideLeft.m_length, vd_angle, tType );
  }
  else if ( &varParameter == &m_params.m_sideLeft.m_angle || // ���� ������ ���� ����� 
    &varParameter == &m_params.m_sideRight.m_angle )
  {
    if ( ::fabs( varParameter.GetDoubleValue() ) < Math::paramEpsilon )
      return 0.0;

    double angle = ::fabs( m_params.m_typeAngle ? m_params.GetAngle() : 180 - m_params.GetAngle() );
    return reader.GetTolerance( M_PI * m_params.m_distance * angle / 180.0, // ����� ����
      vd_angle,
      tType );
  }


  return ShMtBaseBend::GetGeneralTolerance( varParameter, tType, reader );
}


//------------------------------------------------------------------------------------
/// ������ ���� ��� ��������� ������� ���������� �� �������
/**
// \method    InitGeneralTolerances
// \access    public
// \return    void
// \qualifier
*/
//---
void ShMtBend::UpdateMeasurePars( GeneralToleranceType gType, bool recalc )
{
  if ( recalc )
  {
    // � ������ ����� ���������� ������
    double rad = MB_MAXDOUBLE;
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
    {
      double rad_i = m_bendObjects[i]->GetAbsRadius();
      if ( rad_i < rad )
        rad = rad_i;
    }

    m_params.m_distance = rad + GetThickness(); // ������� ������ BUG 62631
  }

  VariableParametersOwner::UpdateMeasurePars( gType, recalc );
}


// *** ���-����� ***************************************************************
//------------------------------------------------------------------------------
// ������� ���-�����
// ---
void ShMtBend::GetHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  if ( type == ActiveHotPoints::hp_none || type == ActiveHotPoints::hp_param )
  {
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendDeepness, editComponent, topComponent, HotPointVectorParam::ht_moveInvert ) ); // �������� ����� 
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendLeftSideLength, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ����������� ����� �����
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendRightSideLength, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ����������� ����� ������
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendLeftSideObjectOffset, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // �������� ��� ����� ������������ ������� �����
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendRightSideObjectOffset, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // �������� ��� ����� ������������ ������� ������

    hotPoints.Add( new HotPointBendAngleParam( ehp_ShMtBendAngle, editComponent, topComponent ) ); // ���� �����
    hotPoints.Add( new HotPointVectorParam( ehp_ShMtBendSideLeftDistance, editComponent, topComponent, HotPointVectorParam::ht_moveInvert ) ); // ������ �� ���� ����� ����� �������
    hotPoints.Add( new HotPointVectorParam( ehp_ShMtBendSideRightDistance, editComponent, topComponent, HotPointVectorParam::ht_moveInvert ) ); // ������ �� ���� ����� ������ �������
    hotPoints.Add( new HotPointVectorParam( ehp_ShMtBendWidth, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ������
    hotPoints.Add( new HotPointDirectionParam( ehp_ShMtBendDirection, editComponent, topComponent ) );
    hotPoints.Add( new HotPointBendRadiusParam( ehp_ShMtBendRadius, editComponent, topComponent ) ); // ������ ����� 
  }
  if ( !GetCircuitParameters().IsAnyCreateParam() && (type == ActiveHotPoints::hp_none || type == ActiveHotPoints::hp_additional) )
  {
    hotPoints.Add( new HotPointVectorParam( ehp_ShMtBendSideLeftWidening, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ����������  ����������� (������ �����) ����� ����� �������
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideRightWidening, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ����������  ����������� (������ �����) ����� ������ �������
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideLeftAngle, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ���� ������ ���� ����� ����� �������
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideRightAngle, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ���� ������ ���� ����� ������ �������
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideLeftDeviation, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ���� ������ ����������� (������ �����) ����� ����� �������
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideRightDeviation, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ���� ������ ����������� (������ �����) ����� ������ �������
  }
  if ( type == ActiveHotPoints::hp_none || type == ActiveHotPoints::hp_thinwall )
  {
    hotPoints.Add( new ArrowHeadPointParam( ehp_ShMtBendWidthBend, editComponent, topComponent, HotPointVectorParam::ht_move ) );     // ������  ��������� �����
    hotPoints.Add( new ArrowHeadPointParam( ehp_ShMtBendDepthBend, editComponent, topComponent/*, HotPointVectorParam::ht_move*/ ) ); // ������� ��������� �����
  }
}


//------------------------------------------------------------------------------
/**
����������� �������������� ���-�����
\param hotPoints -
\param type -
\param pD3DrawTool -
*/
//---
void ShMtBend::UpdateHotPoints( RPArray<HotPointParam> & hotPoints,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  if ( hotPoints.Count() )
  {
    ShMtBendObject * bendParam = nullptr;
    for ( size_t i = 0, count = m_bendObjects.Count(); i < count && !bendParam; ++i )
      bendParam = m_bendObjects[i] && m_bendObjects[i]->m_byOwner ? m_bendObjects[i] : nullptr;

    if ( !bendParam && m_bendObjects.Count() )
      bendParam = m_bendObjects[0];

    MbBendByEdgeValues     params;
    std::unique_ptr<HotPointBendOperUtil> util( GetHotPointUtil( params,
      hotPoints[0]->GetEditComponent( false/*addref*/ ),
      bendParam ) );
    if ( util )
    {
      // ���� �����, ��������� ������� ������� (�� ����� �� ����)
      util->SetLengthBaseObjects( &m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_leftSideLengthBaseObject ),
        &m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_rightSideLengthBaseObject ) );

      for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
      {
        HotPointParam * hotPoint = hotPoints[i];

        switch ( hotPoint->GetIdent() )
        {
        case ehp_ShMtBendDeepness: util->UpdateDeepnessHotPoint( *hotPoint, params, m_params ); break; // �������� ����� 
        case ehp_ShMtBendLeftSideLength: util->UpdateSideLengthHotPoint( *hotPoint, params, m_params, ShMtBendSide::LEFT_SIDE ); break; // ����������� ����� �����
        case ehp_ShMtBendRightSideLength: util->UpdateSideLengthHotPoint( *hotPoint, params, m_params, ShMtBendSide::RIGHT_SIDE ); break; // ����������� ����� ������
        case ehp_ShMtBendLeftSideObjectOffset: util->UpdateSideObjectOffsetHotPoint( *hotPoint, params, m_params, ShMtBendSide::LEFT_SIDE ); break; // �������� ��� ����� ������������ �������� ������� �����
        case ehp_ShMtBendRightSideObjectOffset: util->UpdateSideObjectOffsetHotPoint( *hotPoint, params, m_params, ShMtBendSide::RIGHT_SIDE ); break; // �������� ��� ����� ������������ �������� ������� ������
        case ehp_ShMtBendSideLeftDistance: util->UpdateSideLeftDistanceHotPoint( *hotPoint, params, m_params ); break; // ������ �� ���� ����� ����� �������
        case ehp_ShMtBendSideRightDistance: util->UpdateSideRightDistanceHotPoint( *hotPoint, params, m_params ); break; // ������ �� ���� ����� ������ ������� 
        case ehp_ShMtBendWidth: util->UpdateWidthDistanceHotPoint( *hotPoint, params, m_params ); break; // ������
        case ehp_ShMtBendDirection: util->UpdateDirctionHotPoint( *hotPoint, m_params ); break; // �����������
        case ehp_ShMtBendSideLeftWidening: util->UpdateSideLeftWideningHotPoint( *hotPoint, params, m_params ); break; // ����������  ����������� (������ �����) ����� ����� �������
        case ehp_ShMtBendSideRightWidening: util->UpdateSideRightWideningHotPoint( *hotPoint, params, m_params ); break; // ����������  ����������� (������ �����) ����� ������ ������� 
        case ehp_ShMtBendWidthBend: util->UpdateWidthDismissalHotPoint( *hotPoint, params, m_params ); break; // ������ ��������� �����
        case ehp_ShMtBendDepthBend: util->UpdateDepthDismissalHotPoint( *hotPoint, params, m_params ); break; // ������� ��������� �����

        case ehp_ShMtBendAngle: // ���� �����
        {
          ((HotPointBendAngleParam *)hotPoint)->params = params;
          util->UpdateAngleHotPoint( *hotPoint, params, m_params );
        }
        break;

        case ehp_ShMtBendRadius: // ������ �����
        {
          // ���-����� ������� ���������� ������ ���� ���� �������� �� ���������� ����� ��������
          if ( bendParam && bendParam->m_byOwner )
          {
            ((HotPointBendRadiusParam *)hotPoint)->params = params;
            util->UpdateRadiusHotPoint( *hotPoint, params, m_params );
          }
          else
            hotPoint->SetBasePoint( nullptr );
        }
        break;

        case ehp_ShMtBendSideLeftAngle: // ���� ������ ���� ����� ����� �������
        {
          ((HotPointBendParam*)hotPoint)->params = params;
          util->UpdateSideLeftAngleHotPoint( *hotPoint, params, m_params );
        }
        break;

        case ehp_ShMtBendSideRightAngle: // ���� ������ ���� ����� ������ �������
        {
          ((HotPointBendParam*)hotPoint)->params = params;
          util->UpdateSideRightAngleHotPoint( *hotPoint, params, m_params );
        }
        break;

        case ehp_ShMtBendSideLeftDeviation: // ���� ������ ����������� (������ �����) ����� ����� �������
        {
          ((HotPointBendParam*)hotPoint)->params = params;
          util->UpdateSideLeftDeviationHotPoint( *hotPoint, params, m_params );
        }
        break;

        case ehp_ShMtBendSideRightDeviation: // ���� ������ ����������� (������ �����) ����� ������ �������
        {
          ((HotPointBendParam*)hotPoint)->params = params;
          util->UpdateSideRightDeviationHotPoint( *hotPoint, params, m_params );
        }
        break;
        }
      }
    }
    else
    {
      for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
        hotPoints[i]->SetBasePoint( nullptr );
    }
  }
}


//--------------------------------------------------------------------------------
/**
������ �������������� ���-����� (�� ���-����� ������ ����� ������ ����)
\param hotPoint
\return bool
*/
//---
bool ShMtBend::BeginDragHotPoint( HotPointParam & hotPoint )
{
  return false;
}


//--------------------------------------------------------------------------------
/**
���������� �������������� ���-����� (�� ���-����� �������� ����� ������ ����)
\param hotPoint
\param pD3DrawTool
\return bool
*/
//---
bool ShMtBend::EndDragHotPoint( HotPointParam & hotPoint, D3Draw & pD3DrawTool )
{
  return false;
}


//-----------------------------------------------------------------------------
// ���������� ��� �������������� ���-����� 
/**
\param hotPoint - ��������� ���-�����
\param vector - ������ ������
\param step - ��� ������������, ��� ����������� �������� � �������� ������������� �����
*/
//---
bool ShMtBend::ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
  double step, D3Draw & pD3DrawTool )
{
  bool ret = false;

  switch ( hotPoint.GetIdent() )
  {
  case ehp_ShMtBendDeepness:
  {
    bool dir = m_params.m_typeRemoval == ShMtBaseBendParameters::tr_byIn;
    ret = SetHotPointVectorDirect( (HotPointVectorParam &)hotPoint, vector,
      step, m_params.m_deepness, dir );

    m_params.m_typeRemoval = dir ? ShMtBaseBendParameters::tr_byIn
      : ShMtBaseBendParameters::tr_byOut;
  }
  break;

  case ehp_ShMtBendLeftSideLength:
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideLeft.m_length, 0.00, 5E+5 );
    break;

  case ehp_ShMtBendRightSideLength:
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideRight.m_length, 0.00, 5E+5 );
    break;

  case ehp_ShMtBendLeftSideObjectOffset:
    ret = SetObjectOffsetParamByVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params, ShMtBendSide::LEFT_SIDE );
    break;

  case ehp_ShMtBendRightSideObjectOffset:
    ret = SetObjectOffsetParamByVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params, ShMtBendSide::RIGHT_SIDE );
    break;

  case ehp_ShMtBendAngle:
    ret = SetHotPointAnglePar( (HotPointBendAngleParam &)hotPoint, vector,
      step, m_params );
    break;

  case ehp_ShMtBendSideLeftDistance: // ������ �� ���� ����� ����� �������
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideLeft.m_distance, -5E+5, 5E+5 );
    break;

  case ehp_ShMtBendSideRightDistance: // ������ �� ���� ����� ������ �������
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideRight.m_distance, -5E+5, 5E+5 );
    break;

  case ehp_ShMtBendWidth: // ������
    ret = SetHotPointVector( (HotPointVectorParam&)hotPoint, vector, step,
      m_params.m_width, 0.0002, 5E+5 );
    break;

  case ehp_ShMtBendDirection: // �����������
    ret = ((HotPointDirectionParam &)hotPoint).SetHotPoint( vector,
      m_params.m_dirAngle,
      pD3DrawTool );
    break;

  case ehp_ShMtBendRadius: // ������ �����
    ret = ((HotPointBendRadiusParam &)hotPoint).SetRadiusHotPoint( vector, step,
      m_params, GetThickness() );
    break;

  case ehp_ShMtBendSideLeftWidening: // ����������  ����������� (������ �����) ����� ����� ������� 
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideLeft.m_widening, -5E+5, 5E+5 );
    break;

  case ehp_ShMtBendSideRightWidening: // ����������  ����������� (������ �����) ����� ������ ������� 
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideRight.m_widening, -5E+5, 5E+5 );
    break;

  case ehp_ShMtBendSideLeftAngle: // ���� ������ ���� ����� ����� ������� 
    ret = SetHotPointVectorAngle( (HotPointBendParam &)hotPoint, vector, step,
      m_params.m_sideLeft.m_angle, -90.0, 90.0, GetThickness() );
    break;

  case ehp_ShMtBendSideRightAngle: // ���� ������ ���� ����� ������ ������� 
    ret = SetHotPointVectorAngle( (HotPointBendParam &)hotPoint, vector, step,
      m_params.m_sideRight.m_angle, -90.0, 90.0, GetThickness() );
    break;

  case ehp_ShMtBendSideLeftDeviation: // ���� ������ ����������� (������ �����) ����� ����� �������
    ret = SetHotPointVectorDeviation( (HotPointBendParam &)hotPoint, vector,
      step, m_params.m_sideLeft.m_deviation,
      -90.0, 90.0, ShMtBendSide::LEFT_SIDE );
    break;

  case ehp_ShMtBendSideRightDeviation: // ���� ������ ����������� (������ �����) ����� ������ �������
    ret = SetHotPointVectorDeviation( (HotPointBendParam &)hotPoint, vector,
      step, m_params.m_sideRight.m_deviation,
      -90.0, 90.0, ShMtBendSide::RIGHT_SIDE );
    break;

  case ehp_ShMtBendWidthBend: // ������  ��������� �����
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.GetWidthVar(), 0.00, 5E+5 );
    break;

  case ehp_ShMtBendDepthBend: // �������  ��������� �����
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.GetDepthVar(), 0.00, 5E+5 );
    break;
  }

  return ret;
}


HotPointBendOperUtil * ShMtBend::GetHotPointUtil( MbBendByEdgeValues & params,
  IfComponent         & editComponent,
  ShMtBendObject      * children,
  bool ignoreStraightMode )
{
  return GetHotPointUtil( params, &editComponent, children, ignoreStraightMode );
}


//--------------------------------------------------------------------------------
/**
����������� ����� ��� ��������
\param params
\param editComponent
\param children
\param ignoreStraightMode
\return
*/
//---
HotPointBendOperUtil * ShMtBend::GetHotPointUtil( MbBendByEdgeValues & params,
  IfComponent        * editComponent,
  ShMtBendObject     * children,
  bool ignoreStraightMode )
{
  HotPointBendOperUtil * util = nullptr;

  IfSomething * obj = nullptr;//GetBendObject().obj;
  _ASSERT( m_bendObjects.Count() == GetBendCount() );
  size_t index = 0;
  for ( size_t count = m_bendObjects.Count(); index < count && !obj; ++index )
    obj = m_bendObjects[index] && children == m_bendObjects[index] ? GetBend( index ).obj : nullptr;

  if ( !obj )
  {
    obj = GetBendObject().obj;
    index = 1;
  }

  IFPTR( PartEdge ) partEdge( obj );
  if ( partEdge )
  {
    MbCurveEdge * curveEdge = partEdge->GetMathEdge();
    if ( curveEdge )
    {
      m_params.ReadBaseIfChanged( editComponent ); // ���� ��������� ��������� ����� �� ����, �� ���������� ����, ���� ����
      m_params.GetValues( params, curveEdge->GetMetricLength(), GetObjVersion() );
      if ( GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0800000BL )
        // �������� ��������� ����� ���� ����������
        m_params.CalcAfterValues( params );

      if ( children )
      {
        CreatorForBendUnfoldParams _creatorBends( *this, editComponent );
        _creatorBends.Build( index, children->m_extraName );
        _creatorBends.GetParams( params, m_params.m_dirAngle, index - 1 );
      }

      if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
        // �������� ��������� ����� ���� ����������
        m_params.CalcAfterValues( params );

      // � ��������� ����������� ���������� ������������ ��������� ������������
      util = new HotPointBendOperUtil( *curveEdge, params, ignoreStraightMode ? false : IsStraighten(), GetThickness() );
    }
  }

  return util;
}


//--------------------------------------------------------------------------------
/**
������� ���-����� (��������������� ������� � 5000 �� 5999)
\param hotPoints
\param editComponent
\param topComponent
\param children
\return void
*/
//---
void ShMtBend::GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ShMtBendObject &         children )
{
  hotPoints.Add( new HotPointBendRadiusParam( ehp_ShMtBendObjectRadius, editComponent,
    topComponent ) ); // ������ ����� 
}


//-----------------------------------------------------------------------------
// ���������� ��� �������������� ���-����� 
/**
\param hotPoint - ��������������� ���-�����
\param vector - ������ ��������
\param step - ��� ������������, ��� ����������� �������� � �������� ������������� �����
\param children - ����, ��� �������� ���������� ���-�����
*/
//---
bool ShMtBend::ChangeChildrenHotPoint( HotPointParam &  hotPoint,
  const MbVector3D &     vector,
  double           step,
  ShMtBendObject & children )
{
  bool ret = false;

  if ( hotPoint.GetIdent() == ehp_ShMtBendObjectRadius )            // ������ �����
    ret = ((HotPointBendRadiusParam &)hotPoint).SetRadiusHotPoint( vector, step, children, GetThickness() );

  return ret;
}


//------------------------------------------------------------------------------
/**
����� �� � ������� ���� ��������� ����������� �������
\return ����� �� � ������� ���� ��������� ����������� �������
*/
//---
bool ShMtBend::IsDimensionsAllowed() const
{
  return true;
}


//--------------------------------------------------------------------------------
/**
�������� ���-����� ��� ��������
\param hotPoints
\param editComponent
\param topComponent
\param allHotPoints
\return void
*/
//---
void ShMtBend::GetHotPointsForDimensions( RPArray<HotPointParam> & hotPoints,
  IfComponent & editComponent,
  IfComponent & topComponent,
  bool allHotPoints /*= false*/ )
{
  if ( allHotPoints ) {
    GetHotPoints( hotPoints, editComponent, topComponent, ActiveHotPoints::hp_none, ::GetD3DrawTool() );
    GetHotPoints( hotPoints, editComponent, topComponent, ActiveHotPoints::hp_param, ::GetD3DrawTool() );
    GetHotPoints( hotPoints, editComponent, topComponent, ActiveHotPoints::hp_thinwall, ::GetD3DrawTool() );
  }
  else {
    ShMtBaseBend::GetHotPointsForDimensions( hotPoints, editComponent, topComponent, allHotPoints );
  }
}


//------------------------------------------------------------------------------
/**
������������� ������� � ����������� ��������
\param dimensions - �������
*/
//---
void ShMtBend::AssignDimensionsVariables( const SFDPArray<PartObject> & dimensions )
{
  ShMtBaseBend::AssignDimensionsVariables( dimensions );
  ::UpdateDimensionVar( dimensions, ed_radius, m_params.m_radius );
  ::UpdateDimensionVar( dimensions, ed_angle, m_params.GetAngleVar() );
  ::UpdateDimensionVar( dimensions, ed_leftSideLength, m_params.m_sideLeft.m_length );
  ::UpdateDimensionVar( dimensions, ed_rightSideLength, m_params.m_sideRight.m_length );
  ::UpdateDimensionVar( dimensions, ed_leftSideObjectOffset, m_params.m_sideLeft.m_objectOffset );
  ::UpdateDimensionVar( dimensions, ed_rightSideObjectOffset, m_params.m_sideRight.m_objectOffset );
  ::UpdateDimensionVar( dimensions, ed_deepness, m_params.m_deepness );
  ::UpdateDimensionVar( dimensions, ed_width, m_params.m_width );
  ::UpdateDimensionVar( dimensions, ed_indentLeft, m_params.m_sideLeft.m_distance );
  ::UpdateDimensionVar( dimensions, ed_indentRight, m_params.m_sideRight.m_distance );
  ::UpdateDimensionVar( dimensions, ed_wideningLeft, m_params.m_sideLeft.m_widening );
  ::UpdateDimensionVar( dimensions, ed_wideningRight, m_params.m_sideRight.m_widening );
  ::UpdateDimensionVar( dimensions, ed_deviationLeft, m_params.m_sideLeft.m_deviation );
  ::UpdateDimensionVar( dimensions, ed_deviationRight, m_params.m_sideRight.m_deviation );
  ::UpdateDimensionVar( dimensions, ed_widthRelease, m_params.GetWidthVar() );
  ::UpdateDimensionVar( dimensions, ed_depthRelease, m_params.GetDepthVar() );
  ::UpdateDimensionVar( dimensions, ed_leftAngle, m_params.m_sideLeft.m_angle );
  ::UpdateDimensionVar( dimensions, ed_rightAngle, m_params.m_sideRight.m_angle );
}


//------------------------------------------------------------------------------
/**
������� ������� ������� �� ���-������
\param un - �����������
\param dimensions - �������
\param hotPoints - ���-�����
*/
//---
void ShMtBend::CreateDimensionsByHotPoints( IfUniqueName * un, SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints )
{
  for ( size_t i = 0, count = hotPoints.Count(); i < count; ++i )
  {
    _ASSERT( hotPoints[i] != nullptr );
    switch ( hotPoints[i]->GetIdent() )
    {
    case ehp_ShMtBendRadius:
      ::AttachGnrRadialDimension( dimensions, *hotPoints[i], ed_radius );
      break;
    case ehp_ShMtBendAngle:
      ::AttachGnrAngularDimension( dimensions, *hotPoints[i], ed_angle );
      break;
    case ehp_ShMtBendLeftSideLength:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_leftSideLength );
      break;
    case ehp_ShMtBendRightSideLength:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_rightSideLength );
      break;
    case ehp_ShMtBendLeftSideObjectOffset:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_leftSideObjectOffset );
      break;
    case ehp_ShMtBendRightSideObjectOffset:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_rightSideObjectOffset );
      break;
    case ehp_ShMtBendDeepness:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_deepness );
      break;
    case ehp_ShMtBendWidth:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_width );
      break;
    case ehp_ShMtBendSideLeftDistance:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_indentLeft );
      break;
    case ehp_ShMtBendSideRightDistance:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_indentRight );
      break;
    case ehp_ShMtBendSideLeftWidening:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_wideningLeft );
      break;
    case ehp_ShMtBendSideRightWidening:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_wideningRight );
      break;
    case ehp_ShMtBendSideLeftDeviation:
      ::AttachGnrAngularDimension( dimensions, *hotPoints[i], ed_deviationLeft );
      break;
    case ehp_ShMtBendSideRightDeviation:
      ::AttachGnrAngularDimension( dimensions, *hotPoints[i], ed_deviationRight );
      break;
    case ehp_ShMtBendWidthBend:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_widthRelease );
      break;
    case ehp_ShMtBendDepthBend:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_depthRelease );
      break;
    case ehp_ShMtBendSideLeftAngle:
      ::AttachGnrAngularDimension( dimensions, *hotPoints[i], ed_leftAngle );
      break;
    case ehp_ShMtBendSideRightAngle:
      ::AttachGnrAngularDimension( dimensions, *hotPoints[i], ed_rightAngle );
      break;
    }
  }

  ShMtBaseBend::CreateDimensionsByHotPoints( un, dimensions, hotPoints );
}


//------------------------------------------------------------------------------
/**
�������� ��������� �������� ������� �� ���-������
\param dimensions - �������
\param hotPoints - ���-�����
*/
//---
void ShMtBend::UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  const PArray<OperationSpecificDispositionVariant>* pVariants )
{
  ShMtBaseBend::UpdateDimensionsByHotPoints( dimensions, hotPoints, pVariants );

  if ( hotPoints.Count() == 0 )
    return;

  // #84969 � ������ ������� � �������� ������ �������
  if ( GetPhantomError() != rt_Success && GetPhantomError() != -1 )
  {
    for ( size_t i = 0, c = dimensions.Count(); i < c; ++i )
      dimensions[i]->ResetDimensionGeometry();
    return;
  }

  ShMtBendObject * bendParam = nullptr;
  for ( size_t i = 0, count = m_bendObjects.Count(); i < count && !bendParam; ++i )
    bendParam = m_bendObjects[i] && m_bendObjects[i]->m_byOwner ? m_bendObjects[i] : nullptr;

  if ( !bendParam && m_bendObjects.Count() )
    bendParam = m_bendObjects[0];

  MbBendByEdgeValues params;
  std::unique_ptr<HotPointBendOperUtil> util( GetHotPointUtil( params, hotPoints[0]->GetEditComponent( false ), bendParam ) );
  if ( !!util )
  {
    // ���� �����, ��������� ������� ������� (�� ����� �� ����)
    util->SetLengthBaseObjects( &m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_leftSideLengthBaseObject ),
      &m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_rightSideLengthBaseObject ) );
    if ( m_straighten )
    {
      ResetRadiusDimension( dimensions, ed_radius );
      ResetAngleDimension( dimensions );
    }
    else
    {
      UpdateRadiusDimension( dimensions, *util, ed_radius, !m_params.m_internal );
      UpdateAngleDimension( dimensions, *util );
    }

    UpdateSideLengthDimension( dimensions, *util, ShMtBendSide::LEFT_SIDE );
    UpdateSideLengthDimension( dimensions, *util, ShMtBendSide::RIGHT_SIDE );
    UpdateSideObjectOffsetDimension( dimensions, *util, ShMtBendSide::LEFT_SIDE );
    UpdateSideObjectOffsetDimension( dimensions, *util, ShMtBendSide::RIGHT_SIDE );
    UpdateDeepnessDimension( dimensions, *util );
    UpdateWidthDimension( dimensions, *util, hotPoints );
    UpdateIndentDimension( dimensions, *util, hotPoints, ehp_ShMtBendSideLeftDistance, ed_indentLeft, m_params.m_sideLeft.m_distance );
    UpdateIndentDimension( dimensions, *util, hotPoints, ehp_ShMtBendSideRightDistance, ed_indentRight, m_params.m_sideRight.m_distance );
    UpdateWideningDimensions( dimensions, *util, hotPoints );
    UpdateDeviationDimensions( dimensions, *util, hotPoints );
    UpdateWidthReleaseDimension( dimensions, *util, hotPoints );
    UpdateDepthReleaseDimension( dimensions, *util, hotPoints );
    UpdateSideAngleDimensions( dimensions, *util, hotPoints );
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ �������
\param dimensions -
*/
//--- 
void ShMtBend::ResetRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions, EDimensionsIds dimensionId )
{
  IFPTR( OperationCircularDimension ) radiusDimension( FindGnrDimension( dimensions, dimensionId, false ) );
  if ( radiusDimension != nullptr )
    radiusDimension->ResetSupportingArc();
}


//------------------------------------------------------------------------------
/**
�������� ������ ����
\param dimensions -
*/
//--- 
void ShMtBend::ResetAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions )
{
  IFPTR( OperationAngularDimension ) angleDimension( FindGnrDimension( dimensions, ed_angle, false ) );
  if ( angleDimension != nullptr )
    angleDimension->ResetDimensionPosition();
}


//------------------------------------------------------------------------------
/**
������������� ������� �������� �������� - ������
\param dimensions -
\param children -
*/
//---
void ShMtBend::AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
  ShMtBendObject & children )
{
  ShMtBaseBend::AssingChildrenDimensionsVariables( dimensions, children );

  ::UpdateDimensionVar( dimensions, ed_childrenRadius, children.m_radius );
}


//------------------------------------------------------------------------------
/**
������� ������� ������� �� ���-������ ��� �������� ��������
\param un -
\param dimensions -
\param hotPoints -
*/
//---
void ShMtBend::CreateChildrenDimensionsByHotPoints( IfUniqueName * un,
  SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  ShMtBendObject & children )
{
  for ( size_t i = 0, count = hotPoints.Count(); i < count; ++i )
  {
    _ASSERT( hotPoints[i] );
    if ( hotPoints[i]->GetIdent() == ehp_ShMtBendObjectRadius && !children.m_byOwner )
    {
      ::AttachGnrRadialDimension( dimensions, *hotPoints[i], ed_childrenRadius );
    }
  }
  ShMtBaseBend::CreateChildrenDimensionsByHotPoints( un, dimensions, hotPoints, children );
}



//------------------------------------------------------------------------------
/**
�������� ��������� �������� ������� �� ���-������ ��� �������� ��������
\param dimensions -
\param hotPoints -
\param children -
*/
//---
void ShMtBend::UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  ShMtBendObject & children )
{
  ShMtBaseBend::UpdateChildrenDimensionsByHotPoints( dimensions, hotPoints, children );
  if ( m_straighten )
  {
    ResetRadiusDimension( dimensions, ed_childrenRadius );
  }
  else
  {
    if ( !children.m_byOwner )
    {
      MbBendByEdgeValues params;
      HotPointBendOperUtil * util = GetHotPointUtil( params, hotPoints[0]->GetEditComponent( false ), &children/*m_bendObjects.Count() ? m_bendObjects[0] : nullptr*/ );
      if ( util != nullptr )
      {
        UpdateRadiusDimension( dimensions, *util, ed_childrenRadius, !children.m_internal );
        delete util;
        util = nullptr;
      }
    }
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ���������� ���� �����
\return ������ ���������� ���� �����
*/
//--- 
double ShMtBend::GetBendInternalLength( const HotPointBendOperUtil & pUtils ) const
{
  if ( pUtils.m_pDeepness == nullptr || pUtils.m_pAngle == nullptr )
    return 0;

  double bendInternalLength = 0;
  if ( m_straighten )
  {
    bendInternalLength = pUtils.m_pDeepness->m_pPoint.DistanceToPoint( pUtils.m_pAngle->m_pPoint );
  }
  else
  {
    bendInternalLength = pUtils.m_pDeepness->m_pPoint.DistanceToPoint( pUtils.m_pAngle->m_pPoint ) * m_params.GetAbsAngle();
  }

  return bendInternalLength;
}


//------------------------------------------------------------------------------
/**
�������� ������ ���� ������ ������ �����
\param dimensions - �������
\param utils - ��������� ���-�����
\param hotPoints - ���-����� ��������
*/
//---
void ShMtBend::UpdateSideAngleDimensions( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints ) const
{
  IFPTR( OperationAngularDimension ) leftSideAngleDimension( FindGnrDimension( dimensions, ed_leftAngle, false ) );
  if ( leftSideAngleDimension != nullptr )
  {
    MbCartPoint3D dimensionCenter;
    MbCartPoint3D dimensionPoint1;
    MbCartPoint3D dimensionPoint2;
    bool parametersCalculateSuccess = GetSideAngleDimensionParameters( utils, hotPoints, GetBendInternalLength( utils ) / cos( m_params.m_sideLeft.m_angle * Math::DEGRAD ),
      ehp_ShMtBendSideLeftAngle, m_params.m_sideLeft.m_angle, dimensionCenter, dimensionPoint1, dimensionPoint2 );

    if ( parametersCalculateSuccess )
    {
      leftSideAngleDimension->SetDimensionPosition( dimensionCenter, dimensionPoint1, dimensionPoint2 );
      leftSideAngleDimension->SetDimensionType( MdeAngDimTypes::MIN_ANGLE );
    }
    else
    {
      leftSideAngleDimension->ResetDimensionPosition();
    }
  }

  IFPTR( OperationAngularDimension ) rightSideAngleDimension( FindGnrDimension( dimensions, ed_rightAngle, false ) );
  if ( rightSideAngleDimension != nullptr )
  {
    MbCartPoint3D dimensionCenter;
    MbCartPoint3D dimensionPoint1;
    MbCartPoint3D dimensionPoint2;
    bool parametersCalculateSuccess = GetSideAngleDimensionParameters( utils, hotPoints, GetBendInternalLength( utils ) / cos( m_params.m_sideRight.m_angle * Math::DEGRAD ),
      ehp_ShMtBendSideRightAngle, m_params.m_sideRight.m_angle, dimensionCenter, dimensionPoint1, dimensionPoint2 );

    if ( parametersCalculateSuccess )
    {
      rightSideAngleDimension->SetDimensionPosition( dimensionCenter, dimensionPoint1, dimensionPoint2 );
      rightSideAngleDimension->SetDimensionType( MdeAngDimTypes::MIN_ANGLE );
    }
    else
    {
      rightSideAngleDimension->ResetDimensionPosition();
    }
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ���� ������ ������� �����
\param utils - ��������� ���-�����
\param hotPoints - ���-����� ��������
\param dimensionLength - ������ �������� �����
\param hotPointId - id ���-����� ��������� � ��������
\param angle - �������� ���� ������
\param dimensionCenter - ����������� ����� ������� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
*/
//---
bool ShMtBend::GetSideAngleDimensionParameters( const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints,
  double dimensionLength,
  ShMtBendHotPoints hotPointId,
  double angle,
  MbCartPoint3D & dimensionCenter,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 ) const
{
  /*                     toCenterX
  <-----O______________
  |    /
  |   /
  toCenterY |  / |dimensionLength|
  | /
  V/
  * center
  /
  /
  /__________________________
  |
  |
  */
  if ( utils.m_pLength == nullptr || utils.m_pAngle == nullptr )
    return false;

  HotPointVectorParam * sideAngleHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( hotPointId, hotPoints ));
  if ( sideAngleHotPoint == nullptr )
    return false;

  MbCartPoint3D * sideAngleHotPointPosition = sideAngleHotPoint->GetBasePoint();
  if ( sideAngleHotPointPosition == nullptr )
    return false;

  MbVector3D invertedSideAngleDirection = sideAngleHotPoint->m_norm;
  invertedSideAngleDirection.Invert();

  MbVector3D toCenterX = invertedSideAngleDirection * (sin( angle * Math::DEGRAD ) * dimensionLength);

  MbVector3D lengthToAngle( utils.m_pLength->m_norm );
  lengthToAngle.Invert();
  lengthToAngle.Normalize();
  MbVector3D toCenterY = lengthToAngle * dimensionLength * cos( angle * Math::DEGRAD );

  MbCartPoint3D center = *sideAngleHotPointPosition + toCenterX + toCenterY;

  MbVector3D sideAngleToCenter( *sideAngleHotPointPosition, center );
  sideAngleToCenter.Normalize();
  MbCartPoint3D point1 = center + sideAngleToCenter;
  MbCartPoint3D point2 = center + lengthToAngle;

  dimensionCenter = center;
  dimensionPoint1 = point1;
  dimensionPoint2 = point2;
  return true;
}


//------------------------------------------------------------------------------
/**
�������� ������ ������������ - �������
\param dimensions - ������� ��������
\param utils - ��������� ���-�����
\param hotPoints - ���-����� ��������
*/
//---
void ShMtBend::UpdateDepthReleaseDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints )
{
  IFPTR( OperationLinearDimension ) depthReleaseDimension( FindGnrDimension( dimensions, ed_depthRelease, false ) );
  if ( depthReleaseDimension == nullptr )
    return;

  MbPlacement3D placement;
  MbCartPoint3D point1;
  MbCartPoint3D point2;
  bool parametersCalculateSuccess = GetDepthReleaseDimensionParameters( utils, hotPoints, placement, point1, point2 );

  if ( m_params.m_bendBendNo || !parametersCalculateSuccess )
  {
    depthReleaseDimension->ResetDimensionPosition();
  }
  else
  {
    depthReleaseDimension->SetDimensionPlacement( placement, false );
    depthReleaseDimension->SetDimensionPosition( point1, point2 );
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ������������ �������
\param utils - ��������� ���-�����
\param hotPoints - ���-����� ��������
\param dimensionPlacement - ��������� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
*/
//---
bool ShMtBend::GetDepthReleaseDimensionParameters( const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 )
{
  if ( utils.m_pDeepness == nullptr || utils.m_pRadius == nullptr || utils.m_pAngle == nullptr || utils.m_placeBend == nullptr )
    return false;

  HotPointVectorParam * depthReleaseHotPoint = dynamic_cast< HotPointVectorParam *>(FindHotPointWithId( ehp_ShMtBendDepthBend, hotPoints ));
  if ( depthReleaseHotPoint == nullptr )
    return false;

  MbCartPoint3D * depthReleasePosition = depthReleaseHotPoint->GetBasePoint();
  if ( depthReleasePosition == nullptr )
    return false;

  MbCartPoint3D point1 = *depthReleasePosition;
  MbCartPoint3D point2 = *depthReleasePosition - depthReleaseHotPoint->m_norm * m_params.GetDepthVar();

  MbVector3D axisX( point1, point2 );
  MbVector3D axisZ( -utils.m_placeBend->GetAxisY() );
  if ( !axisX.Colinear( axisZ, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXZ( point1, axisX, axisZ );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ������������ - ������
\param dimensions - ������� ��������
\param utils - ��������� ���-�����
\param hotPoints - ���-����� ��������
*/
//---
void ShMtBend::UpdateWidthReleaseDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints )
{
  IFPTR( OperationLinearDimension ) widthReleaseDimension( FindGnrDimension( dimensions, ed_widthRelease, false ) );
  if ( widthReleaseDimension == nullptr )
    return;

  MbPlacement3D placement;
  MbCartPoint3D point1;
  MbCartPoint3D point2;
  bool parametersCalculateSuccess = GetWidthReleaseDimensionParameters( utils, hotPoints, placement, point1, point2 );

  if ( m_params.m_bendBendNo || !parametersCalculateSuccess )
  {
    widthReleaseDimension->ResetDimensionPosition();
  }
  else
  {
    widthReleaseDimension->SetDimensionPlacement( placement, false );
    widthReleaseDimension->SetDimensionPosition( point1, point2 );
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ������������ ������
\param pDimensions - ������� ��������
\param pUtils - ��������� ���-�����
\param pHotPoints - ���-����� ��������
\param pDimensionPlacement - ��������� �������
\param pDimensionPoint1 - ����� 1 �������
\param pDimensionPoint2 - ����� 2 �������
*/
//---
bool ShMtBend::GetWidthReleaseDimensionParameters( const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 )
{
  if ( utils.m_pDeepness == nullptr )
    return false;

  HotPointVectorParam * widthReleaseHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( ehp_ShMtBendWidthBend, hotPoints ));
  if ( widthReleaseHotPoint == nullptr )
    return false;

  MbCartPoint3D * widthReleasePosition = widthReleaseHotPoint->GetBasePoint();
  if ( widthReleasePosition == nullptr )
    return false;

  MbCartPoint3D point1 = *widthReleasePosition;
  MbCartPoint3D point2 = *widthReleasePosition - widthReleaseHotPoint->m_norm * m_params.GetWidthVar();
  MbVector3D axisX( point1, point2 );
  MbVector3D axisY( utils.m_pDeepness->m_norm );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}

//------------------------------------------------------------------------------
/**
�������� ������� ������ ����������� �����
\param dimensions - ������� ��������
\param utils - ��������� ���-�����
\param hotPoints - ���-����� ��������
*/
//---
void ShMtBend::UpdateDeviationDimensions( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints ) const
{
  // ����� � ������ ������� ����������� �.�. ��� ����������� ��������� �������� ����� ����������
  if ( m_params.m_sideLeft.m_typeSide == ShMtBendSide::ts_angle )
  {
    IFPTR( OperationAngularDimension ) leftDimension( FindGnrDimension( dimensions, ed_deviationLeft, false ) );
    if ( leftDimension != nullptr )
    {
      MbCartPoint3D dimensionCenter;
      MbCartPoint3D dimensionPoint1;
      MbCartPoint3D dimensionPoint2;
      bool parametersCalculateSuccess = GetDeviationDimensionParameters( utils, hotPoints, ehp_ShMtBendSideLeftDeviation, ehp_ShMtBendSideLeftAngle,
        dimensionCenter, dimensionPoint1, dimensionPoint2 );

      if ( parametersCalculateSuccess )
      {
        leftDimension->SetDimensionType( MdeAngDimTypes::MIN_ANGLE );
        leftDimension->SetDimensionPosition( dimensionCenter, dimensionPoint1, dimensionPoint2 );
      }
      else
      {
        leftDimension->ResetDimensionPosition();
      }
    }
  }

  if ( m_params.m_sideRight.m_typeSide == ShMtBendSide::ts_angle )
  {
    IFPTR( OperationAngularDimension ) rightDimension( FindGnrDimension( dimensions, ed_deviationRight, false ) );
    if ( rightDimension != nullptr )
    {
      MbCartPoint3D dimensionCenter;
      MbCartPoint3D dimensionPoint1;
      MbCartPoint3D dimensionPoint2;
      bool parametersCalculateSuccess = GetDeviationDimensionParameters( utils, hotPoints, ehp_ShMtBendSideRightDeviation, ehp_ShMtBendSideRightAngle,
        dimensionCenter, dimensionPoint1, dimensionPoint2 );

      if ( parametersCalculateSuccess )
      {
        rightDimension->SetDimensionType( MdeAngDimTypes::MIN_ANGLE );
        rightDimension->SetDimensionPosition( dimensionCenter, dimensionPoint1, dimensionPoint2 );
      }
      else
      {
        rightDimension->ResetDimensionPosition();
      }
    }
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ������ �����������
\param dimensions - �������
\param utils - ��������� ���-�����
\param hotPoints - ���-�����
\param deviationHotPointId - id ���-����� ������ �����������
\param sideAngleHotPointId - id ���-����� ������ �����
\param dimensionCenter - ����������� ����� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
*/
//---
bool ShMtBend::GetDeviationDimensionParameters( const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints,
  ShMtBendHotPoints deviationHotPointId,
  ShMtBendHotPoints sideAngleHotPointId,
  MbCartPoint3D & dimensionCenter,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 ) const
{
  if ( utils.m_pLength == nullptr || utils.m_pAngle == nullptr )
    return false;

  MbCartPoint3D * deviationPointPosition = FindHotPointPosition( hotPoints, deviationHotPointId );
  MbCartPoint3D * sideAnglePointPosition = FindHotPointPosition( hotPoints, sideAngleHotPointId );
  if ( deviationPointPosition == nullptr || sideAnglePointPosition == nullptr )
    return false;

  MbCartPoint3D center = *sideAnglePointPosition;

  MbVector3D deviationToSideAngle( *deviationPointPosition, *sideAnglePointPosition );
  deviationToSideAngle.Normalize();
  MbCartPoint3D point1 = center + deviationToSideAngle;

  MbVector3D lengthPointToAnglePoint( utils.m_pLength->m_pPoint, utils.m_pAngle->m_pPoint );
  lengthPointToAnglePoint.Normalize();
  MbCartPoint3D point2 = center + lengthPointToAnglePoint;

  dimensionCenter = center;
  dimensionPoint1 = point1;
  dimensionPoint2 = point2;
  return true;
}


//------------------------------------------------------------------------------
/**
�������� ������ ���������� �����
\param pDimensions - ������� ��������
\param pUtils - ��������� ���-�����
\param pHotPoints - ���-����� ��������
*/
//---
void ShMtBend::UpdateWideningDimensions( const SFDPArray<GenerativeDimensionDescriptor> & pDimensions,
  const HotPointBendOperUtil & pUtils,
  const RPArray<HotPointParam> & pHotPoints )
{
  // ����� � ������ ������� ����������� �.�. ��� ����������� ��������� �������� ����� ����������
  if ( m_params.m_sideLeft.m_typeSide == ShMtBendSide::ts_widening )
  {
    IFPTR( OperationLinearDimension ) leftDimension( FindGnrDimension( pDimensions, ed_wideningLeft, false ) );
    if ( leftDimension != nullptr )
    {
      MbPlacement3D leftPlacement;
      MbCartPoint3D leftPoint1;
      MbCartPoint3D leftPoint2;
      bool parametersCalculateSuccess = GetWideningDimensionParameters( pUtils, pHotPoints, ehp_ShMtBendSideLeftWidening,
        m_params.m_sideLeft.m_widening, leftPlacement, leftPoint1, leftPoint2 );

      if ( parametersCalculateSuccess )
      {
        leftDimension->SetDimensionPlacement( leftPlacement, false );
        leftDimension->SetDimensionPosition( leftPoint1, leftPoint2 );
      }
      else
      {
        leftDimension->ResetDimensionPosition();
      }
    }
  }

  if ( m_params.m_sideRight.m_typeSide == ShMtBendSide::ts_widening )
  {
    IFPTR( OperationLinearDimension ) rightDimension( FindGnrDimension( pDimensions, ed_wideningRight, false ) );
    if ( rightDimension != nullptr )
    {
      MbPlacement3D rightPlacement;
      MbCartPoint3D rightPoint1;
      MbCartPoint3D rightPoint2;
      bool parametersCalculateSuccess = GetWideningDimensionParameters( pUtils, pHotPoints, ehp_ShMtBendSideRightWidening,
        m_params.m_sideRight.m_widening, rightPlacement, rightPoint1, rightPoint2 );

      if ( parametersCalculateSuccess )
      {
        rightDimension->SetDimensionPlacement( rightPlacement, false );
        rightDimension->SetDimensionPosition( rightPoint1, rightPoint2 );
      }
      else
      {
        rightDimension->ResetDimensionPosition();
      }
    }
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ����������
\param utils - ��������� ���-�����
\param hotPoints - ���-�����
\param wideningHotPointId - id ���-����� ������� ���������� ��� �������� ������������� ���������
\param widening - �������� ����������
\param dimensionPlacement - ��������� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
*/
//--- 
bool ShMtBend::GetWideningDimensionParameters( const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints,
  ShMtBendHotPoints wideningHotPointId,
  double widening,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 )

{
  if ( utils.m_pDeepness == nullptr || utils.m_pRadius == nullptr || utils.m_pAngle == nullptr || utils.m_placeBend == nullptr )
    return false;

  HotPointVectorParam * wideningHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( wideningHotPointId, hotPoints ));
  if ( wideningHotPoint == nullptr )
    return false;
  if ( wideningHotPoint->GetBasePoint() == nullptr )
    return false;

  MbCartPoint3D point1 = *wideningHotPoint->GetBasePoint();
  MbCartPoint3D point2 = *wideningHotPoint->GetBasePoint() - wideningHotPoint->m_norm * widening;

  MbVector3D axisY;
  if ( m_straighten )
  {
    axisY = utils.m_placeBend->GetAxisY();
  }
  else
  {
    MbArc3D supportArc( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, 1, true );
    axisY.Init( utils.m_pAngle->m_pPoint, supportArc.GetCentre() );
  }

  MbVector3D axisX( point1, point2 );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ������� �����. ����� ��� ������, ������������ �� id ���-����� � �������
\param pDimensions - ������� ��������
\param pUtils - ��������� ���-�����
\param pHotPoints - ���-����� ��������
\param pHotPointId - id ���-����� ��������� � ��������
\param pDimensionId - id �������
\param pIndent - �������� ������
*/
//---
void ShMtBend::UpdateIndentDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints,
  const ShMtBendHotPoints hotPointId,
  const EDimensionsIds dimensionId,
  double indent ) const
{
  MbCartPoint3D dimensionPoint1;
  MbCartPoint3D dimensionPoint2;
  MbPlacement3D dimensionPlacement;
  bool parametersCalculateSuccess = GetIndentDimensionParameters( utils, FindHotPointPosition( hotPoints, hotPointId ), indent,
    dimensionPoint1, dimensionPoint2, dimensionPlacement );

  IFPTR( OperationLinearDimension ) indentDimension( FindGnrDimension( dimensions, dimensionId, false ) );
  if ( indentDimension != nullptr )
  {
    if ( parametersCalculateSuccess )
    {
      indentDimension->SetDimensionPlacement( dimensionPlacement, false );
      indentDimension->SetDimensionPosition( dimensionPoint1, dimensionPoint2 );
    }
    else
    {
      indentDimension->ResetDimensionPosition();
    }
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ������� �����
*/
//--- 
bool ShMtBend::GetIndentDimensionParameters( const HotPointBendOperUtil & utils, MbCartPoint3D * indentHotPointPosition, double indent,
  MbCartPoint3D & dimensionPoint1, MbCartPoint3D & dimensionPoint2, MbPlacement3D & dimensionPlacement ) const
{
  if ( utils.m_pDeepness == nullptr || utils.m_pRadius == nullptr || utils.m_pAngle == nullptr || indentHotPointPosition == nullptr || utils.m_placeBend == nullptr )
    return false;

  MbCartPoint3D point1 = *indentHotPointPosition;

  MbVector3D deepnessToIndent( utils.m_pDeepness->m_pPoint, *indentHotPointPosition );
  deepnessToIndent.Normalize();
  MbCartPoint3D point2 = *indentHotPointPosition + deepnessToIndent * indent;

  MbVector3D axisY;
  if ( m_straighten )
  {
    axisY = utils.m_placeBend->GetAxisX();
  }
  else
  {
    MbArc3D supportArc( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, 1, true );
    axisY.Init( utils.m_pDeepness->m_pPoint, supportArc.GetCentre() );
  }

  MbVector3D axisX( point1, point2 );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ������ �����
\param pDimensions - ������� ��������
\param pUtils - ��������� ���-�����
\param pHotPoints - ���-������ ��������
*/
//---
void ShMtBend::UpdateWidthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints ) const
{
  IFPTR( OperationLinearDimension ) widthDimension( FindGnrDimension( dimensions, ed_width, false ) );
  if ( widthDimension == nullptr )
    return;

  MbCartPoint3D dimensionPoint1;
  MbCartPoint3D dimensionPoint2;
  MbPlacement3D dimensionPlacement;

  bool parametersCalculateSuccess = GetWidthDimensionParameters( utils, FindHotPointPosition( hotPoints, ehp_ShMtBendWidth ),
    dimensionPoint1, dimensionPoint2, dimensionPlacement );

  if ( m_params.m_bendDisposal == ShMtBendParameters::bd_allLength || m_params.m_bendDisposal == ShMtBendParameters::bd_two || !parametersCalculateSuccess )
  {
    widthDimension->ResetDimensionPosition();
  }
  else
  {
    widthDimension->SetDimensionPlacement( dimensionPlacement, false );
    widthDimension->SetDimensionPosition( dimensionPoint1, dimensionPoint2 );
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ������ �����
*/
//--- 
bool ShMtBend::GetWidthDimensionParameters( const HotPointBendOperUtil & utils, MbCartPoint3D * widthHotPointPosition,
  MbCartPoint3D & dimensionPoint1, MbCartPoint3D & dimensionPoint2, MbPlacement3D & dimensionPlacement ) const
{
  if ( widthHotPointPosition == nullptr )
    return false;
  if ( utils.m_pDeepness == nullptr || utils.m_pRadius == nullptr || utils.m_pAngle == nullptr || utils.m_placeBend == nullptr )
    return false;

  MbCartPoint3D point1 = *widthHotPointPosition;
  MbVector3D deepnessToWidth( utils.m_pDeepness->m_pPoint, *widthHotPointPosition );
  deepnessToWidth.Normalize();
  MbCartPoint3D point2 = utils.m_pDeepness->m_pPoint - deepnessToWidth *  (m_params.m_width / 2.0);

  MbVector3D axisY;
  if ( m_straighten )
  {
    axisY = utils.m_placeBend->GetAxisX();
  }
  else
  {
    MbArc3D supportArc( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, 1, true );
    axisY.Init( utils.m_pDeepness->m_pPoint, supportArc.GetCentre() );
  }

  MbVector3D axisX( point1, point2 );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ �������� �����
\param dimensions - ������� ��������
\param utils - ��������� ���-�����
*/
//---
void ShMtBend::UpdateDeepnessDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils ) const
{
  IFPTR( OperationLinearDimension ) deepnessDimension( FindGnrDimension( dimensions, ed_deepness, false ) );
  if ( deepnessDimension == nullptr )
    return;

  MbPlacement3D dimensionPlacement;
  MbCartPoint3D dimensionPoint1;
  MbCartPoint3D dimensionPoint2;
  bool parametersCalculateSuccess = GetDeepnessDimensionParameters( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2 );

  if ( parametersCalculateSuccess && (m_params.m_typeRemoval == ShMtBendParameters::tr_byIn || m_params.m_typeRemoval == ShMtBendParameters::tr_byOut) )
  {
    deepnessDimension->SetDimensionPlacement( dimensionPlacement, false );
    deepnessDimension->SetDimensionPosition( dimensionPoint1, dimensionPoint2 );
  }
  else
  {
    deepnessDimension->ResetDimensionPosition();
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� �������� �����
*/
//--- 
bool ShMtBend::GetDeepnessDimensionParameters( const HotPointBendOperUtil & utils, MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1, MbCartPoint3D & dimensionPoint2 ) const
{
  if ( utils.m_pDeepness == nullptr || utils.m_pRadius == nullptr || utils.m_placeBend == nullptr )
    return false;

  MbCartPoint3D point2;
  MbVector3D axisY;
  if ( m_params.m_typeRemoval == ShMtBendParameters::tr_byIn )
  {
    point2 = utils.m_pDeepness->m_pPoint + utils.m_pDeepness->m_norm * m_params.m_deepness;
    axisY = -utils.m_placeBend->GetAxisY();
  }
  else
  {
    if ( m_params.m_typeRemoval == ShMtBendParameters::tr_byOut )
    {
      point2 = utils.m_pDeepness->m_pPoint - utils.m_pDeepness->m_norm * m_params.m_deepness;
      axisY = utils.m_placeBend->GetAxisY();
    }
  }

  MbCartPoint3D point1 = utils.m_pDeepness->m_pPoint;
  MbVector3D axisX( point1, point2 );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ������� ����� ( ��� ��������� ����� )
\param dimensions - ������� ��������
\param utils - ��������� ���-�����
\param dimensionId - id ������� �������
\param moveToExternal - �������� �� ������� ������
*/
//---
void ShMtBend::UpdateRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  EDimensionsIds dimensionId,
  bool moveToExternal )
{
  IFPTR( OperationCircularDimension ) radiusDimension( FindGnrDimension( dimensions, dimensionId, false ) );
  if ( radiusDimension == nullptr )
    return;

  MbArc3D dimensionArc( MbPlacement3D(), 0, 0, 0 );
  MbPlacement3D dimensionPlacement;
  bool parametersCalculateSuccess = GetRadiusDimensionParameters( utils, moveToExternal, dimensionPlacement, dimensionArc );

  if ( parametersCalculateSuccess )
  {
    radiusDimension->SetDimensionPlacement( dimensionArc.GetPlacement() );
    radiusDimension->SetSupportingArc( dimensionArc );
  }
  else
  {
    radiusDimension->ResetSupportingArc();
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ������� ����� ( ��� ��������� ����� )
\return ������� �� �������� ��������� �������
*/
//--- 
bool ShMtBend::GetRadiusDimensionParameters( const HotPointBendOperUtil & utils, bool moveToExternal,
  MbPlacement3D & dimensionPlacement, MbArc3D & dimensionArc )
{
  if ( (utils.m_pDeepness == nullptr) || (utils.m_pRadius == nullptr) || (utils.m_pAngle == nullptr) )
    return false;

  // ����� �������
  MbArc3D arc( utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, utils.m_pDeepness->m_pPoint, 1, true );

  // ����������� �� ������� ������
  if ( moveToExternal )
  {
    arc.SetRadiusA( arc.GetRadiusA() + m_params.m_thickness );
    arc.SetRadiusB( arc.GetRadiusB() + m_params.m_thickness );
  }

  if ( !arc.IsDegenerate() )
  {
    dimensionArc.Init( arc );
    dimensionPlacement = arc.GetPlacement();
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ����� �����
\param dimensions - ������� ��������
\param utils - ��������� ���-�����
*/
//---
void ShMtBend::UpdateLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils ) const
{
  MbPlacement3D dimensionPlacement;
  MbCartPoint3D dimensionPoint1;
  MbCartPoint3D dimensionPoint2;
  bool parametersCalculateSuccess = false;

  switch ( m_params.m_sideLeft.m_absLengthType )
  {
  case ShMtBendSide::AbsLengthType::alt_byContinue:
    parametersCalculateSuccess = GetLengthDimensionParametersByContinue( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2 );
    break;
  case ShMtBendSide::AbsLengthType::alt_byContour:
    parametersCalculateSuccess = GetLengthDimensionParametersByContour( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2 );
    break;
  case ShMtBendSide::AbsLengthType::alt_byTouch:
    parametersCalculateSuccess = GetLengthDimensionParametersByTouch( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2 );
    break;
  default:
    _ASSERT( FALSE );
  };

  IFPTR( OperationLinearDimension ) lengthDimension( FindGnrDimension( dimensions, ed_length, false ) );
  if ( lengthDimension != nullptr )
  {
    if ( parametersCalculateSuccess )
    {
      lengthDimension->SetDimensionPlacement( dimensionPlacement, false );
      lengthDimension->SetDimensionPosition( dimensionPoint1, dimensionPoint2 );
    }
    else
    {
      lengthDimension->ResetDimensionPosition();
    }
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ����� �����
\param pDimensions - ������� ��������
\param pUtils - ��������� ���-�����
\param isLeftSide - ������ ��� ����� ������� �����? ����� ��� ������.
*/
//---
void ShMtBend::UpdateSideLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  bool isLeftSide ) const
{
  IFPTR( OperationLinearDimension ) lengthDimension( FindGnrDimension( dimensions, isLeftSide ? ed_leftSideLength : ed_rightSideLength, false ) );
  if ( lengthDimension == nullptr )
    return;

  MbPlacement3D dimensionPlacement;
  MbCartPoint3D dimensionPoint1;
  MbCartPoint3D dimensionPoint2;
  bool parametersCalculateSuccess = GetSideLengthDimensionParameters( utils, isLeftSide, dimensionPlacement, dimensionPoint1, dimensionPoint2 );

  if ( parametersCalculateSuccess )
  {
    lengthDimension->SetDimensionPlacement( dimensionPlacement, false );
    lengthDimension->SetDimensionPosition( dimensionPoint1, dimensionPoint2 );
  }
  else
  {
    lengthDimension->ResetDimensionPosition();
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ����� �����
*/
//--- 
bool ShMtBend::GetSideLengthDimensionParameters( const HotPointBendOperUtil & utils, bool isLeftSide,
  MbPlacement3D & dimensionPlacement, MbCartPoint3D & dimensionPoint1, MbCartPoint3D & dimensionPoint2 ) const
{
  const ShMtBendSide & bendSide = isLeftSide ? m_params.m_sideLeft : m_params.m_sideRight;

  bool parametersCalculateSuccess = false;
  if ( (bendSide.m_lengthCalcMethod == ShMtBendSide::lcm_absolute) &&
    (isLeftSide || m_params.m_bendLengthBy2Sides) )
  {
    switch ( bendSide.m_absLengthType )
    {
    case ShMtBendSide::alt_byContinue:
    {
      if ( m_params.m_bendLengthBy2Sides )
        parametersCalculateSuccess = GetSideLengthDimensionParametersByContinue( utils, dimensionPlacement, dimensionPoint1,
          dimensionPoint2, isLeftSide );
      else
        parametersCalculateSuccess = GetLengthDimensionParametersByContinue( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2 );

      break;
    }
    case ShMtBendSide::alt_byContour:
    case ShMtBendSide::alt_byContourInternal:
    {
      if ( m_params.m_bendLengthBy2Sides )
        parametersCalculateSuccess = GetSideLengthDimensionParametersByContour( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2,
          bendSide.m_absLengthType == ShMtBendSide::alt_byContourInternal,
          isLeftSide );
      else
        parametersCalculateSuccess = GetLengthDimensionParametersByContour( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2 );

      break;
    }
    case ShMtBendSide::alt_byTouch:
    case ShMtBendSide::alt_byTouchInternal:
    {
      if ( m_params.m_bendLengthBy2Sides )
        parametersCalculateSuccess = GetSideLengthDimensionParametersByTouch( utils, dimensionPlacement, dimensionPoint1,
          dimensionPoint2, bendSide.m_absLengthType == ShMtBendSide::alt_byTouchInternal,
          isLeftSide );
      else
        parametersCalculateSuccess = GetLengthDimensionParametersByTouch( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2 );

      break;
    }
    }
  }

  return parametersCalculateSuccess;
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ����� �� �������
\param utils - ��������� ���-�����
\param dimensionPlacement - �������������� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
\return ������� �� �������� ��������� �������
*/
//---
bool ShMtBend::GetLengthDimensionParametersByTouch( const HotPointBendOperUtil & utils,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 ) const
{
  bool computeLikeContour = false;
  if ( m_params.m_typeAngle )
    computeLikeContour = m_params.GetAngle() < M_PI_2 * Math::RADDEG;
  else
    computeLikeContour = m_params.GetAngle() >= M_PI_2 * Math::RADDEG;

  if ( computeLikeContour )
    return GetLengthDimensionParametersByContour( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2 );
  else
    return GetLengthDimensionObtuseAngle( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2 );
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ����� �� ������� ��� ���� ������ 90
\param utils - ��������� ���-�����
\param dimensionPlacement - �������������� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
\return ������� �� �������� ��������� �������
*/
//---
bool ShMtBend::GetLengthDimensionObtuseAngle( const HotPointBendOperUtil & utils,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 ) const
{
  if ( (utils.m_pAngle == nullptr) || (utils.m_pLength == nullptr) || (utils.m_pDeepness == nullptr) || (utils.m_pRadius == nullptr) || (utils.m_placeBend == nullptr) )
    return false;

  MbCartPoint3D point1 = utils.m_pLength->m_pPoint;
  if ( m_params.m_sideLeft.m_absLengthType == ShMtBendSide::alt_byContourInternal ||
    m_params.m_sideLeft.m_absLengthType == ShMtBendSide::alt_byTouchInternal )
  {
    point1 = utils.m_pLength->m_pPoint;
  }
  else
  {
    MbVector3D thicknessDirection;
    if ( m_straighten )
    {
      MbPlacement3D bendPlacement;
      GetPlacement( &bendPlacement );
      thicknessDirection = -bendPlacement.GetAxisZ();
    }
    else
    {
      MbArc3D supportArc( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, 1, true );
      MbVector3D centerToAnglePoint( supportArc.GetCentre(), utils.m_pAngle->m_pPoint );
      centerToAnglePoint.Normalize();
      thicknessDirection = centerToAnglePoint;
    }
    point1 = utils.m_pLength->m_pPoint + thicknessDirection * m_params.m_thickness;
  }

  MbCartPoint3D point2 = point1 - utils.m_pLength->m_norm * m_params.m_sideLeft.m_length;

  MbVector3D axisY;
  if ( m_straighten )
    axisY = -utils.m_placeBend->GetAxisY();
  else
    axisY.Init( utils.m_pDeepness->m_pPoint, utils.m_pAngle->m_pPoint );

  MbVector3D axisX( point1, point2 );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ����� �� �������
\param pUtils - ��������� ���-�����
\param pDimensionPlacement - �������������� �������
\param pDimensionPoint1 - ����� 1 �������
\param pDimensionPoint2 - ����� 2 �������
\return ������� �� ���������� ��������� �������
*/
//---
bool ShMtBend::GetLengthDimensionParametersByContour( const HotPointBendOperUtil & utils,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 ) const
{
  if ( (utils.m_pAngle == nullptr) || (utils.m_pLength == nullptr) || (utils.m_pDeepness == nullptr) || (utils.m_pRadius == nullptr) || (utils.m_placeBend == nullptr) )
    return false;

  MbCartPoint3D point1;
  if ( m_params.m_sideLeft.m_absLengthType == ShMtBendSide::alt_byContourInternal ||
    m_params.m_sideLeft.m_absLengthType == ShMtBendSide::alt_byTouchInternal )
  {
    point1 = utils.m_pLength->m_pPoint;
  }
  else
  {
    MbVector3D thicknessDirection;
    if ( m_straighten )
    {
      MbPlacement3D bendPlacement;
      GetPlacement( &bendPlacement );
      thicknessDirection = -bendPlacement.GetAxisZ();
    }
    else
    {
      MbArc3D supportArc( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, 1, true );
      MbVector3D centerToAnglePoint( supportArc.GetCentre(), utils.m_pAngle->m_pPoint );
      centerToAnglePoint.Normalize();
      thicknessDirection = centerToAnglePoint;
    }
    point1 = utils.m_pLength->m_pPoint + thicknessDirection * m_params.m_thickness;
  }

  MbCartPoint3D point2;
  point2 = point1 - utils.m_pLength->m_norm * m_params.m_sideLeft.m_length;

  MbVector3D axisY;
  if ( m_straighten )
    axisY = -utils.m_placeBend->GetAxisY();
  else
    axisY.Init( utils.m_pDeepness->m_pPoint, utils.m_pAngle->m_pPoint );

  MbVector3D axisX( point1, point2 );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    dimensionPlacement.InitXY( point1, axisX, axisY, true );;
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ����� �� �����������
\param utils - ��������� ���-�����
\param dimensionPlacement - �������������� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
\return ������� �� �������� ��������� �������
*/
//---
bool ShMtBend::GetLengthDimensionParametersByContinue( const HotPointBendOperUtil & utils,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 ) const
{
  if ( (utils.m_pAngle == nullptr) || (utils.m_pLength == nullptr) || (utils.m_pDeepness == nullptr) || (utils.m_placeBend == nullptr) )
    return false;

  MbCartPoint3D point1 = utils.m_pLength->m_pPoint;
  MbCartPoint3D point2 = utils.m_pAngle->m_pPoint;

  MbVector3D axisY;
  if ( m_straighten )
    axisY = utils.m_placeBend->GetAxisY();
  else
    axisY.Init( utils.m_pAngle->m_pPoint, utils.m_pDeepness->m_pPoint );

  MbVector3D axisX( point1, point2 );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    return true;
  }
  else
  {
    return false;
  }
}



//--------------------------------------------------------------------------------
/**
�������� ��������� ������� ����� �� ����������� �����/������
\return ������� �� �������� ��������� �������
*/
//---
bool ShMtBend::GetSideLengthDimensionParametersByContinue( const HotPointBendOperUtil & utils,
  MbPlacement3D        & dimensionPlacement,
  MbCartPoint3D        & dimensionPoint1,
  MbCartPoint3D        & dimensionPoint2,
  bool                   isLeftSide ) const
{
  if ( (utils.m_pAngle == nullptr) ||
    (utils.m_pLength == nullptr) ||
    (utils.m_pDeepness == nullptr) ||
    (utils.m_placeBend == nullptr) ||
    (utils.m_pLeftSideLengthOrigin == nullptr) ||
    (utils.m_pLeftSideLengthEndpoint == nullptr) ||
    (utils.m_pRightSideLengthOrigin == nullptr) ||
    (utils.m_pRightSideLengthEndpoint == nullptr) )
    return false;

  MbCartPoint3D point1, point2;

  utils.GetSideLengthEndpoint( point1, isLeftSide );
  utils.GetSideLengthOriginPoint( point2, true, isLeftSide );

  MbVector3D axisY;
  if ( m_straighten )
    axisY = utils.m_placeBend->GetAxisY();
  else
    axisY.Init( utils.m_pAngle->m_pPoint, utils.m_pDeepness->m_pPoint );

  MbVector3D axisX( point1, point2 );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//--------------------------------------------------------------------------------
/**
�������� ��������� ������� ����������� ����� �����/������
\return ������� �� ���������� ��������� �������
*/
//---
bool ShMtBend::GetSideLengthDimensionParametersByContour( const HotPointBendOperUtil & utils,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2,
  bool isInternal,
  bool isLeftSide ) const
{
  if ( (utils.m_pAngle == nullptr) ||
    (utils.m_pLength == nullptr) ||
    (utils.m_pDeepness == nullptr) ||
    (utils.m_pRadius == nullptr) ||
    (utils.m_pLeftSideLengthOrigin == nullptr) ||
    (utils.m_pLeftSideLengthEndpoint == nullptr) ||
    (utils.m_pRightSideLengthOrigin == nullptr) ||
    (utils.m_pRightSideLengthEndpoint == nullptr) ||
    (utils.m_placeBend == nullptr) )
    return false;

  MbCartPoint3D endPoint;
  utils.GetSideLengthEndpoint( endPoint, isLeftSide );
  const ShMtBendSide & sideParams = isLeftSide ? m_params.m_sideLeft : m_params.m_sideRight;

  MbCartPoint3D point1;
  if ( sideParams.m_absLengthType == ShMtBendSide::alt_byContourInternal ||
    sideParams.m_absLengthType == ShMtBendSide::alt_byTouchInternal )
    point1 = endPoint;
  else
  {
    MbVector3D thicknessDirection;
    if ( m_straighten )
    {
      MbPlacement3D bendPlacement;
      GetPlacement( &bendPlacement );
      thicknessDirection = -bendPlacement.GetAxisZ();
    }
    else
    {
      MbArc3D supportArc( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, 1, true );
      MbVector3D centerToAnglePoint( supportArc.GetCentre(), utils.m_pAngle->m_pPoint );
      centerToAnglePoint.Normalize();
      thicknessDirection = centerToAnglePoint;
    }
    point1 = endPoint + thicknessDirection * m_params.m_thickness;
  }

  MbCartPoint3D point2;
  point2 = point1 - utils.m_pLength->m_norm * sideParams.m_length;

  MbVector3D axisY;
  if ( m_straighten )
    axisY = -utils.m_placeBend->GetAxisY();
  else
    axisY.Init( utils.m_pDeepness->m_pPoint, utils.m_pAngle->m_pPoint );

  MbVector3D axisX( point1, point2 );
  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    return true;
  }
  else
  {
    return false;
  }
}



//--------------------------------------------------------------------------------
/**
������ ���������� ������� ��� ������� ����� ����������� ����� �� �������
\return ������� �� ���������� ��������� �������
*/
//---
bool ShMtBend::GetSideLengthDimensionParametersByTouch( const HotPointBendOperUtil & utils,
  MbPlacement3D       & dimensionPlacement,
  MbCartPoint3D       & dimensionPoint1,
  MbCartPoint3D       & dimensionPoint2,
  bool                  isInternal,
  bool                  isLeftSide ) const
{
  // �� ����� ���� ������� � ���� �� ������� "�� �������" �������, ������� �������� ������� ��� ����
  // ������� ������: � pUtils, � ������� �������� �����.
  return GetSideLengthDimensionParametersByContour( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2, isInternal, isLeftSide );
}


//------------------------------------------------------------------------------
/**
�������� ����������� � ���� � �����
\param pSupportArc - ���� ���������� ����� ���-�����
\param pPointOnArc - ����� �� ���� (��������� ��������)
\param pTangent - �����������
*/
//---
void ShMtBend::GetArcTangent( const MbArc3D & pSupportArc,
  const MbCartPoint3D & pPointOnArc,
  MbLine3D & pTangent ) const
{
  double pointOnArcT = 0;
  pSupportArc.NearPointProjection( pPointOnArc, pointOnArcT, true, nullptr );

  MbVector3D tangetAtPoint;
  pSupportArc.Tangent( pointOnArcT, tangetAtPoint );
  tangetAtPoint.Normalize();

  pTangent.Init( pPointOnArc, tangetAtPoint );
}


//------------------------------------------------------------------------------
/**
�������� ������ ���� �����
\param dimensions - ������� ��������
\param utils - ��������� ���-�����
*/
//---
void ShMtBend::UpdateAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils ) const
{
  IFPTR( OperationAngularDimension ) angleDimension( FindGnrDimension( dimensions, ed_angle, false ) );
  if ( angleDimension == nullptr )
    return;

  MbCartPoint3D dimensionCenter;
  MbCartPoint3D dimensionPoint1;
  MbCartPoint3D dimensionPoint2;
  bool parametersCalculateSuccess = false;

  if ( m_params.m_typeAngle )
    parametersCalculateSuccess = GetAngleDimensionBend( utils, dimensionCenter, dimensionPoint1, dimensionPoint2 );
  else
    parametersCalculateSuccess = GetAngleDimensionSupplementary( utils, dimensionCenter, dimensionPoint1, dimensionPoint2 );

  if ( parametersCalculateSuccess )
  {
    angleDimension->SetDimensionType( ::CalcAngularGnrDimensionType( m_params.GetAngle() * Math::DEGRAD ) );
    angleDimension->SetDimensionPosition( dimensionCenter, dimensionPoint1, dimensionPoint2 );
  }
  else
  {
    angleDimension->ResetDimensionPosition();
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ����, ��� �����������
\param utils - ��������� ���-�����
\param dimensionCenter - ����� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
\return ������� �� �������� ��������� �������
*/
//---
bool ShMtBend::GetAngleDimensionSupplementary( const HotPointBendOperUtil & utils,
  MbCartPoint3D & dimensionCenter,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 ) const
{
  if ( (utils.m_pAngle == nullptr) || (utils.m_pDeepness == nullptr) || (utils.m_pRadius == nullptr) )
    return false;

  bool calculateSuccess = false;
  MbVector3D supportArcAxisX( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint );
  MbVector3D supportArcAxisY( utils.m_pDeepness->m_pPoint, utils.m_pAngle->m_pPoint );
  if ( !supportArcAxisX.Colinear( supportArcAxisY, PARAM_EPSILON ) )
  {
    MbArc3D supportArc( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, 1, true );
    if ( !supportArc.IsDegenerate() )
    {
      MbVector3D centerToDeepnessPoint( supportArc.GetCentre(), utils.m_pDeepness->m_pPoint );
      centerToDeepnessPoint.Normalize();
      MbCartPoint3D point1 = supportArc.GetCentre() + centerToDeepnessPoint;

      MbVector3D anglePointToCenter( utils.m_pAngle->m_pPoint, supportArc.GetCentre() );
      anglePointToCenter.Normalize();
      MbCartPoint3D point2;
      point2 = supportArc.GetCentre() + anglePointToCenter;

      dimensionCenter = supportArc.GetCentre();
      dimensionPoint1 = point1;
      dimensionPoint2 = point2;
      calculateSuccess = true;
    }
  }
  return calculateSuccess;
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ����, ��� - ���� �����
\param utils - ��������� ���-�����
\param dimensionCenter - ����� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
\return ������� �� �������� ��������� �������
*/
//---
bool ShMtBend::GetAngleDimensionBend( const HotPointBendOperUtil & utils,
  MbCartPoint3D & dimensionCenter,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 ) const
{
  if ( (utils.m_pAngle == nullptr) || (utils.m_pDeepness == nullptr) || (utils.m_pRadius == nullptr) )
    return false;

  bool calculateSuccess = false;
  MbVector3D supportArcAxisX( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint );
  MbVector3D supportArcAxisY( utils.m_pDeepness->m_pPoint, utils.m_pAngle->m_pPoint );
  if ( !supportArcAxisX.Colinear( supportArcAxisY, PARAM_EPSILON ) )
  {
    MbArc3D supportArc( utils.m_pDeepness->m_pPoint, utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, 1, true );

    MbVector3D deepnessPointToCenter( utils.m_pDeepness->m_pPoint, supportArc.GetCentre() );
    deepnessPointToCenter.Normalize();
    MbCartPoint3D point1;
    point1 = supportArc.GetCentre() + deepnessPointToCenter;

    MbVector3D anglePointToCenter( utils.m_pAngle->m_pPoint, supportArc.GetCentre() );
    anglePointToCenter.Normalize();
    MbCartPoint3D point2;
    point2 = supportArc.GetCentre() + anglePointToCenter;

    if ( !supportArc.IsDegenerate() )
    {
      dimensionCenter = supportArc.GetCentre();
      dimensionPoint1 = point1;
      dimensionPoint2 = point2;
      calculateSuccess = true;
    }
  }

  return calculateSuccess;
}


//--------------------------------------------------------------------------------
/**
��������� ��������� ������� �������� ������������ �������� ������� ��� ����������� ����� �����/������
\return ������� �� �������� ��������� �������
*/
//---
bool ShMtBend::GetSideObjectOffsetDimensionParameters( const HotPointBendOperUtil & pUtils,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2,
  bool isLeftSide ) const
{
  MbCartPoint3D      basePoint, hpPosition;
  MbVector3D         hpVector;
  bool calculateSuccess = false;

  if ( pUtils.CalculateSideObjectOffsetParams( basePoint, hpPosition, hpVector, m_params, isLeftSide ) && !m_straighten )
  {
    MbVector3D axisY;
    axisY.Init( pUtils.m_pAngle->m_pPoint, pUtils.m_pDeepness->m_pPoint );

    MbVector3D axisX( basePoint, hpPosition );
    if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
    {
      dimensionPoint1 = basePoint;
      dimensionPoint2 = hpPosition;
      dimensionPlacement.InitXY( basePoint, axisX, axisY, true );
      calculateSuccess = true;
    }
  }

  return calculateSuccess;
}


//------------------------------------------------------------------------------
/**
�������� ������ �������� ������������ �������� ������� ��� ����������� ����� �����/������
*/
//---
void ShMtBend::UpdateSideObjectOffsetDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  bool isLeftSide ) const
{
  IFPTR( OperationLinearDimension ) offsetDimension( FindGnrDimension( dimensions, isLeftSide ? ed_leftSideObjectOffset : ed_rightSideObjectOffset, false ) );
  if ( offsetDimension == nullptr )
    return;

  MbPlacement3D dimensionPlacement;
  MbCartPoint3D dimensionPoint1;
  MbCartPoint3D dimensionPoint2;
  bool parametersCalculateSuccess = false;

  bool isSideDefinedByObject = isLeftSide ? m_params.m_sideLeft.IsLengthDefinedByObject() : m_params.m_sideRight.IsLengthDefinedByObject();

  if ( isSideDefinedByObject && GetRefObject( isLeftSide ? IfShMtBend::RefObjIdxs::roi_leftSideLengthBaseObject : IfShMtBend::RefObjIdxs::roi_rightSideLengthBaseObject ) )
    parametersCalculateSuccess = GetSideObjectOffsetDimensionParameters( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2, isLeftSide );


  if ( parametersCalculateSuccess )
  {
    offsetDimension->SetDimensionPlacement( dimensionPlacement, false );
    offsetDimension->SetDimensionPosition( dimensionPoint1, dimensionPoint2 );
  }
  else
  {
    offsetDimension->ResetDimensionPosition();
  }
}


//-----------------------------------------------------------------------------
// ����������� �������������� ���-�����
/**
\param hotPoints - ���-�����
\param children - ����, ��� �������� ���������� ���-�����
*/
//---
void ShMtBend::UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  ShMtBendObject &         children )
{
  if ( hotPoints.Count() )
  {
    bool del = true;

    // ���-����� ������� ���������� ������ ���� ���� �������� �� ���������� ������ �����
    if ( !children.m_byOwner )
    {
      MbBendByEdgeValues params;
      HotPointBendOperUtil * util = GetHotPointUtil( params,
        hotPoints[0]->GetEditComponent( false/*addref*/ ),
        &children );
      if ( util )
      {
        del = false;
        for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
        {
          HotPointParam * hotPoint = hotPoints[i];
          if ( hotPoint->GetIdent() == ehp_ShMtBendObjectRadius ) // ������ �����
          {
            HotPointBendRadiusParam * hotPointBend = (HotPointBendRadiusParam *)hotPoint;
            hotPointBend->params = params;
            util->UpdateRadiusHotPoint( *hotPoint, params, m_params );
          }
        }

        delete util;
      }
    }

    if ( del )
      for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
      {
        hotPoints[i]->SetBasePoint( nullptr );
      }
  }
}
// *** ���-����� ***************************************************************


//--------------------------------------------------------------------------------
/**
������ �� ������
\param in
\param obj
\return void
*/
//---
void ShMtBend::Read( reader &in, ShMtBend * obj )
{
  ReadBase( in, (ShMtBaseBend *)obj );

  if ( in.AppVersion() >= 0x10000004L )
  {
    // ������� ������� ��� ������� ����� ����������� �����
    in >> obj->m_refObjects;
  }

  in >> obj->m_params;  // ��������� �������� �����

  if ( in.AppVersion() >= 0x1100003FL )
    in >> obj->m_typeCircuitBend;
}


//--------------------------------------------------------------------------------
/**
������ � �����
\param out
\param obj
\return void
*/
//---
void ShMtBend::Write( writer &out, const ShMtBend * obj )
{
  if ( out.AppVersion() < 0x06000033L )
    out.setState( io::cantWriteObject ); // �� ����� � ������ ������
  else
  {
    WriteBase( out, (const ShMtBaseBend *)obj );

    if ( out.AppVersion() >= 0x10000004L )
    {
      // ������� ������� ��� ������� ����� ����������� �����
      out << obj->m_refObjects;
    }

    out << obj->m_params;  // ��������� �������� �����

    if ( out.AppVersion() >= 0x1100003FL )
      out << obj->m_typeCircuitBend;
  }
}


//--------------------------------------------------------------------------------
/**
�������� ����� ������
\param prr
\return void
*/
//---
void ShMtBend::AdditionalReadingEnd( PartRebuildResult & prr )
{
  ShMtBaseBend::AdditionalReadingEnd( prr );
  IFPTR( UniqueName ) un( const_cast<IfComponent*>(&prr.GetComponentOwner()) );
  if ( un )
  {
    if ( m_params.m_sideLeft.m_length.IsParameterNameEmpty() )
      m_params.m_sideLeft.m_length.UpdateUniqueName( un->MakeUniqueVarName() );
    if ( m_params.m_sideRight.m_length.IsParameterNameEmpty() )
      m_params.m_sideRight.m_length.UpdateUniqueName( un->MakeUniqueVarName() );
    if ( m_params.m_sideLeft.m_objectOffset.IsParameterNameEmpty() )
      m_params.m_sideLeft.m_objectOffset.UpdateUniqueName( un->MakeUniqueVarName() );
    if ( m_params.m_sideRight.m_objectOffset.IsParameterNameEmpty() )
      m_params.m_sideRight.m_objectOffset.UpdateUniqueName( un->MakeUniqueVarName() );
  }
}


//--------------------------------------------------------------------------------
/**
���������� �� ���������� ��� �������
\param version
\return bool
*/
//---
bool ShMtBend::IsNeedSaveAsUnhistored( long version ) const
{
  if ( version < 0x10000004L && m_params.m_bendLengthBy2Sides )
  {

    if ( m_params.m_sideLeft.m_lengthCalcMethod == ShMtBendSide::LengthCalcMethod::lcm_absolute &&
      m_params.m_sideRight.m_lengthCalcMethod == ShMtBendSide::LengthCalcMethod::lcm_absolute &&
      m_params.m_sideLeft.m_absLengthType == m_params.m_sideRight.m_absLengthType )
    {
      double len1 = 0.0;
      m_params.m_sideLeft.m_length.GetValue( len1 );
      double len2 = 0.0;
      m_params.m_sideRight.m_length.GetValue( len2 );
      if ( ::abs( len1 - len2 ) <= Math::paramDeltaMin )
        return false; // ��� ��������� � ����������������
    }

    // ���� �� ������, ����� �� ������ �������������� � ������ ������.
    // ��������� ��� �������.
    return true;
  }

  if ( version < 0x11000033L && GetBendCount() > 1 )
    return true;

  if ( version < 0x1100003DL && m_params.IsAnyCreateParam() )
    return true;

  return ShMtBaseBend::IsNeedSaveAsUnhistored( version );
}


//--------------------------------------------------------------------------------
/**
���������� ������� ������
\param wrapper
\param index
\return void
*/
//---
void ShMtBend::SetRefObject( const WrapSomething * wrapper, size_t index )
{
  m_refObjects.SetObject( index, wrapper );
}


//--------------------------------------------------------------------------------
/**
�������� ������� ������
\param index
\return const WrapSomething &
*/
//---
const WrapSomething & ShMtBend::GetRefObject( size_t index ) const
{
  return m_refObjects.GetObjectTrans( index );
}


//--------------------------------------------------------------------------------
/**
���������� ���������� �� �������� ������� ��� ������� ����� � ������ ��������
\param utils
\param isLeftSide
\param length
\return const bool
*/
//---
const bool ShMtBend::CalculateSideLengthToObject( HotPointBendOperUtil & utils, bool isLeftSide, double & length )
{
  MbCartPoint3D endPoint; // �������� ����� �����
  MbVector3D    axisVec;  // ������ ������
  MbCartPoint3D cp;
  if ( utils.CalculateSideObjectOffsetParams( cp, endPoint, axisVec, m_params, isLeftSide ) )
  {
    MbCartPoint3D originPoint;
    utils.GetSideLengthOriginPoint( originPoint, m_params.m_bendLengthBy2Sides, isLeftSide );

    // ���������� ����������� ���������
    MbPlacement3D _place( originPoint, axisVec );
    MbPlane dividingPlane( _place );

    MbeItemLocation placementRes = dividingPlane.PointRelative( endPoint, ANGLE_EPSILON );
    double movedVertexDist = originPoint.DistanceToPoint( endPoint );

    // �������� ������ ���� ����� ��������� "���" ��� "�" ��������� �����
    bool positiveDist = (placementRes == MbeItemLocation::iloc_InItem || placementRes == MbeItemLocation::iloc_OnItem);
    if ( !positiveDist )
      movedVertexDist = -movedVertexDist;
    length = movedVertexDist;
    return length >= 0;
  }

  // ������ ������������� �����: ������ �� ������
  length = -1;
  return false;
}


//------------------------------------------------------------------------------
/**
�������������� ��� ���������� � ������ 5.11
*/
//--- 
void ShMtBend::ConvertTo5_11( IfCanConvert & owner, const WritingParms & wrParms )
{
  // ���� �� ��������: ���� ������� ����� �� ����������� � 5.11
}


//------------------------------------------------------------------------------
/**
�������������� ��� ���������� � ����������� ������
*/
//--- 
void ShMtBend::ConvertToSavePrevious( SaveDocumentVersion saveMode, IfCanConvert & owner, const WritingParms & wrParms )
{
  if ( m_params.m_bendLengthBy2Sides )
  {
    // ������������, ��� ��� ����� ������ ��������� � �������������� �����.
    // � ��������� ������ ������ ����������� ����� ��������.
    m_params.m_bendLengthBy2Sides = false;
    // ����� ����������, ����� ������� ����� ����� ��������.
  }
  else
  {
    if ( m_params.m_sideLeft.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute )
    {
      m_params.m_sideLeft.m_lengthCalcMethod = ShMtBendSide::LengthCalcMethod::lcm_absolute;
      // �����! ������ ��� ������� ����� �� ����� ���� ����� ����� (�������������)
      // � ��������� ������ ��� ������������ ����������� ����� ���������� �� �����.
      m_params.m_sideLeft.m_absLengthType = ShMtBendSide::alt_byContinue;
    }
  }
  // KOMPAS-23638   BEGIN
  if ( saveMode < SaveDocumentVersion::sdv_Kompas_18
    && m_params.m_typeRemoval == ShMtBaseBendParameters::tr_byCentre )
  {                                     // �������� ������ ���������� "�� ����� �����" �� "�������� ������"
    m_params.m_typeRemoval = ShMtBaseBendParameters::tr_byIn;

    MbBendValues mbParams;              // �������� ����� ��� ���� ��������� ������
    ((ShMtBaseBendParameters)m_params).GetValues( mbParams );

    IFC_Array<ShMtBendObject> children; // �������� ��������
    GetChildrens( children );
    if ( children.Count() )             // KOMPAS-23920: ��������� ����� ������ �������� ������� ��������
      mbParams.radius = children[0]->GetAbsRadius();

    MbDisplacementCalculator dCalc( m_params.m_thickness, mbParams.radius, mbParams.angle, mbParams.k );
    m_params.m_deepness = ::fabs( dCalc.CalcDisplacement( MbDisplacementCalculator::MbeDisplacementType::dt_Center ) );
  }                                     // KOMPAS-23638   END
}


IfCanConvert * ShMtBend::CreateConverter( IfCanConvert & what )
{
  return nullptr;
}


//------------------------------------------------------------------------------
/**
������� �� �������������� ��� ���������� � ������ 5.11
*/
//--- 
bool ShMtBend::IsNeedConvertTo5_11( SSArray<uint> & types, IfCanConvert & owner )
{
  // � 5.11 ������ ����� �� �����������
  return false;
}


//------------------------------------------------------------------------------
/**
������� �� �������������� ��� ���������� � ����������� ������
*/
//--- 
bool ShMtBend::IsNeedConvertToSavePrevious( SSArray<uint> & types, SaveDocumentVersion saveMode, IfCanConvert & owner )
{
  bool shouldConvert = false; // ����� (� �����) �� �������������� � ������ ������?
  if ( saveMode <= SaveDocumentVersion::sdv_Kompas_15_Sp2 )
  {
    // ��� ����� ��������������, �� ����� �� ��?
    // �������������� � ������ ������ ����� ����� ��� �������, ��� � ��� ���������� ���� (�� �� 2-� ��������),
    // ���� ��� ������� ������������, ��� ���� ����� �������� � ���������� �����.
    // ��� �� ����� ������ �������� �������� ��, ��� ������ ������� ����� ������ ���������.
    if ( m_params.m_bendLengthBy2Sides &&
      m_params.m_sideLeft.m_lengthCalcMethod == ShMtBendSide::LengthCalcMethod::lcm_absolute &&
      m_params.m_sideRight.m_lengthCalcMethod == ShMtBendSide::LengthCalcMethod::lcm_absolute &&
      m_params.m_sideLeft.m_absLengthType == m_params.m_sideRight.m_absLengthType )
    {
      // ������������ ������ ���� ��� ����� �������� ��������� � ��� �������������� �����:
      double len1 = 0.0;
      m_params.m_sideLeft.m_length.GetValue( len1 );
      double len2 = 0.0;
      m_params.m_sideRight.m_length.GetValue( len2 );
      if ( ::abs( len1 - len2 ) <= Math::paramDeltaMin )
        shouldConvert = true;
    }
    else if ( !m_params.m_bendLengthBy2Sides && m_params.m_sideLeft.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute ) // ���������� ����
      shouldConvert = true;
  }
  // KOMPAS-23638
  if ( !shouldConvert && saveMode < SaveDocumentVersion::sdv_Kompas_18 && m_params.m_typeRemoval == ShMtBaseBendParameters::tr_byCentre )
    shouldConvert = true;

  if ( shouldConvert )
    types.Add( co_shmtBend );

  return shouldConvert;
}


//--------------------------------------------------------------------------------
/**
��������� ����� ������ ����������� �����, ���� ��� ������ �� � ���������� ����
\param params
\return bool
*/
//---
bool ShMtBend::UpdateSideLengthsForNonAbsoluteCalcMode( MbBendByEdgeValues & params )
{
  bool suitableParamValues = true; // ���� ����� ����������� � ������ �����-�� ������������ ��������, ��������,
                                   // ����� ������� (����� �� ������������ ���������) ����� � ��������������� ������� �� ����������� �����.

  std::auto_ptr<HotPointBendOperUtil> utils( GetHotPointUtil( params,
    m_bendObject.GetObjectTrans( 0 ).editComponent,
    m_bendObjects.Count() ? m_bendObjects[0] : nullptr,
    true ) );
  // VB ����� utils ����� �� ��������� �� null. ���� ������ ����������� ������� �����, �� �������� ��� �������.
  utils->SetLengthBaseObjects( &m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_leftSideLengthBaseObject ),
    &m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_rightSideLengthBaseObject ) );

  if ( m_params.m_sideLeft.IsLengthDefinedByObject() )
  {
    suitableParamValues &= CalculateSideLengthToObject( *utils.get(), ShMtBendSide::LEFT_SIDE, params.sideLeft.length );
    m_params.m_sideLeft.m_length = params.sideLeft.length;
    if ( !m_params.m_bendLengthBy2Sides )
    {
      params.sideRight.length = params.sideLeft.length;
      m_params.m_sideRight.m_length = params.sideLeft.length;
    }
  }

  if ( m_params.m_sideRight.IsLengthDefinedByObject() && m_params.m_bendLengthBy2Sides )
  {
    suitableParamValues &= CalculateSideLengthToObject( *utils.get(), ShMtBendSide::RIGHT_SIDE, params.sideRight.length );
    m_params.m_sideRight.m_length = params.sideRight.length;
  }

  return suitableParamValues;
}


//--------------------------------------------------------------------------------
/**
���� �� ����������� �� ����������� �������?
\param relType
\param parent
\return bool
*/
//---
bool ShMtBend::IsThisParent( uint8 relType, const WrapSomething & parent ) const
{
  return ShMtBaseBend::IsThisParent( relType, parent ) || (relType == rlt_Strong ? false : m_refObjects.IsThisParent( relType, parent ));
}


//--------------------------------------------------------------------------------
/**
���������, ������� �� ������� ������� ��� ������������ ���� ������ �����
\return bool ��� �� � ������� � �������� ���������
*/
//---
bool ShMtBend::IsValidBaseObjects()
{
  bool res = true;

  // ��������� ����� �������
  if ( m_params.m_sideLeft.IsLengthDefinedByObject() )
  {
    const WrapSomething & objectWrp = m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_leftSideLengthBaseObject );
    if ( m_params.m_sideLeft.m_lengthCalcMethod == ShMtBendSide::lcm_toVertex )
      res &= !!IFPTR( AnyPoint )(objectWrp.obj);
    else if ( m_params.m_sideLeft.m_lengthCalcMethod == ShMtBendSide::lcm_toPlane )
    {
      IFPTR( AnySurface ) planeObj( objectWrp.obj );
      res &= (planeObj && planeObj->IsPlanar());
    }
  }

  // ��������� ������ �������
  if ( m_params.m_bendLengthBy2Sides && m_params.m_sideRight.IsLengthDefinedByObject() )
  {
    const WrapSomething & objectWrp = m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_rightSideLengthBaseObject );
    if ( m_params.m_sideRight.m_lengthCalcMethod == ShMtBendSide::lcm_toVertex )
      res &= !!IFPTR( AnyPoint )(objectWrp.obj);
    else if ( m_params.m_sideRight.m_lengthCalcMethod == ShMtBendSide::lcm_toPlane )
    {
      IFPTR( AnySurface ) planeObj( objectWrp.obj );
      res &= (planeObj && planeObj->IsPlanar());
    }
  }

  return res;
}


//--------------------------------------------------------------------------------
/**
�������� �������������� ������� ������������ ��������� �����.
*/
//---
MbeItemLocation ShMtBend::BendObjectLocation( const WrapSomething & wrap, bool forLeftSide )
{
  MbeItemLocation result = iloc_Unknown;

  MbBendByEdgeValues params;
  std::auto_ptr<HotPointBendOperUtil> utils( GetHotPointUtil( params,
    m_bendObject.GetObjectTrans( 0 ).editComponent,
    m_bendObjects.Count() ? m_bendObjects[0] : nullptr,
    true ) );
  IFPTR( AnyPoint ) pt3D( wrap.obj );
  IFPTR( AnySurface ) basePlane( wrap.obj );
  if ( pt3D && !!utils.get() )
  {
    MbCartPoint3D destPoint;
    pt3D->GetCartPoint( destPoint );

    ::TransformFromInto( destPoint,
      wrap.component,
      wrap.editComponent );

    MbCartPoint3D originPoint;
    utils->GetSideLengthOriginPoint( originPoint, false, true );
    MbVector3D directionVec;
    utils->GetSideLengthVector( directionVec );

    // ���������� ����������� ���������
    MbPlacement3D _place( originPoint, directionVec );
    MbPlane dividingPlane( _place );

    result = dividingPlane.PointRelative( destPoint, ANGLE_EPSILON );
  }
  else if ( !!utils.get() && basePlane && basePlane->IsPlanar() )
  {
    MbPlacement3D planePlacement;
    basePlane->GetPlacement( &planePlacement );

    ::TransformFromInto( planePlacement,
      wrap.component,
      wrap.editComponent );

    MbCartPoint3D originPoint;
    utils->GetSideLengthOriginPoint( originPoint, m_params.m_bendLengthBy2Sides, forLeftSide );
    MbVector3D directionVec;
    utils->GetSideLengthVector( directionVec );

    // ������������ �������������
    double projX( UNDEFINED_DBL ), projY( UNDEFINED_DBL );
    if ( planePlacement.DirectPointProjection( originPoint, directionVec, projX, projY ) )
    {
      // �������������, �� ��� ������: � ������ �� ������� �� ������ ����� ���������� ��������?
      MbCartPoint3D projectedPoint;
      planePlacement.PointOn( projX, projY, projectedPoint );

      // ���������� ����������� ���������
      MbPlacement3D _place( originPoint, directionVec );
      MbPlane dividingPlane( _place );

      result = dividingPlane.PointRelative( projectedPoint, ANGLE_EPSILON );
    }
  }

  return result;
}


//--------------------------------------------------------------------------------
/**
����� �� ������������� ����������� ����� ��-�� ���������� �������� �������, �� �������� ������ ����.
*/
//---
bool ShMtBend::IsNeedInvertDirectionBend()
{
  bool invert = false;

  MbeItemLocation locationFirst = m_params.m_sideLeft.IsLengthDefinedByObject() ? BendObjectLocation( m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_leftSideLengthBaseObject ), true ) : iloc_Undefined;
  MbeItemLocation locationSecond = m_params.m_sideRight.IsLengthDefinedByObject() ? BendObjectLocation( m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_rightSideLengthBaseObject ), false ) : iloc_Undefined;

  return (locationFirst == MbeItemLocation::iloc_OutOfItem || locationSecond == MbeItemLocation::iloc_OutOfItem) && locationFirst != MbeItemLocation::iloc_InItem && locationSecond != MbeItemLocation::iloc_InItem;
}



//--------------------------------------------------------------------------------
/**
����� �� ������������ ������ ������ ��� ������� �������?
\param pointWrapper
\return const bool
*/
//---
const bool ShMtBend::IsObjectSuitableAsBaseVertex( const WrapSomething & pointWrapper )
{
  MbeItemLocation location = IFPTR( AnyPoint )(pointWrapper.obj) ? BendObjectLocation( pointWrapper, true/*��� ��� �����*/ ) : MbeItemLocation::iloc_Undefined;

  return location == MbeItemLocation::iloc_InItem || location == MbeItemLocation::iloc_OutOfItem;
}


//--------------------------------------------------------------------------------
/**
����� �� ������������ ������ ������ ��� ������� ���������?
\param pointWrapper
\return const bool
*/
//---
const bool ShMtBend::IsObjectSuitableAsBasePlane( const WrapSomething & planeWrapper, bool forLeftSide )
{
  MbeItemLocation location = IFPTR( AnySurface )(planeWrapper.obj) ? BendObjectLocation( planeWrapper, forLeftSide ) : MbeItemLocation::iloc_Undefined;

  return location == MbeItemLocation::iloc_InItem || location == MbeItemLocation::iloc_OutOfItem;
}


//--------------------------------------------------------------------------------
/**
��������� �������� ����� ������� ����������� �����
\param isLeftSide
\return void
*/
//---
void ShMtBend::FixAbsSideLength( bool isLeftSide )
{
  ShMtBendSide & bendSide = isLeftSide ? m_params.m_sideLeft : m_params.m_sideRight;
  if ( bendSide.m_lengthCalcMethod == ShMtBendSide::lcm_absolute && bendSide.m_length.GetVarValue() < 0 )
    bendSide.m_length = 0.0;
}


//--------------------------------------------------------------------------------
/**
�������� ��� ���������� ������ � ��������
\param refContainers ���������� ������ ��������
\return void
*/
//---
void ShMtBend::GetRefContainers( FDPArray<AbsReferenceContainer> & refContainers )
{
  refContainers.Add( &m_bendObject );
  refContainers.Add( &m_refObjects );
}


////////////////////////////////////////////////////////////////////////////////
//
// ����� �������� ���� �� �����
//
////////////////////////////////////////////////////////////////////////////////
class ShMtBaseBendLine : public IfShMtBendLine,
  public ShMtBaseBend,
  public IfHotPoints
{
public:
  ReferenceContainer      m_bendFaces;      // ����� ������� ����

public:
  ShMtBaseBendLine( IfUniqueName * un );
  ShMtBaseBendLine( const ShMtBaseBendLine & );

public:
  I_DECLARE_SOMETHING_FUNCS;

  virtual bool                              IsObjectExist( const WrapSomething * bend ) const { return ShMtBaseBend::IsObjectExist( bend ); }
  virtual void                              SetParameters( const ShMtBaseBendParameters & ) = 0;
  virtual ShMtBaseBendParameters          & GetParameters() const = 0;

  virtual void                              SetBendObject( const WrapSomething * ) override;
  virtual const WrapSomething &             GetBendObject() const override;
  virtual bool                              SetResetBendFace( const WrapSomething & ) override;
  virtual void                              ResetAllBendFace() override; // ������� ��� face ������� ����
  virtual size_t                            GetBendFaceCount() const override; // �-�� faces ������� ����
  virtual WrapSomething *                   GetBendFace( uint i ) override; // i - face ������� ����
  virtual const WrapSomething *             GetBendFace( uint i ) const override;
  virtual SArray<WrapSomething>             GetBaseFaces() const override;
#ifdef MANY_THICKNESS_BODY_SHMT
  virtual bool                              IsSuitedObject( const WrapSomething & ) override;
#else
  virtual bool                              IsSuitedObject( const WrapSomething &, double ) override;
#endif  
  virtual bool                              IsLeftSideFix() = 0;
  //--------------- ����� ������� ������� ������ -------------------------------//
  virtual void            Assign( const PrObject &other ) override;           // ���������� ��������� �� ������� �������

  virtual bool            RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath ) override; // ������������ �����
  virtual void            ClearReferencedObjects( bool soft ) override;
  virtual bool            ChangeReferences( ChangeReferencesData & changedRefs ) override; ///< �������� ������ �� �������
  virtual bool            IsValid() const override;  // �������� �������������� �������
  virtual void            WhatsWrong( WarningsArray & ); // �������� ������ �������������� �� �������
  virtual bool            IsThisParent( uint8 relType, const WrapSomething & ) const override;
  virtual void            GetVariableParameters( VarParArray &, ParCollType parCollType ) override;
  virtual void            ForgetVariables() override; // �� K6+ ������ ��� ����������, ��������� � �����������
  virtual bool            PrepareToAdding( PartRebuildResult &, PartObject * ) override; //������������� �� ����������� � ������
                                                                                         // �������� ���������� ��� ��������������� Solida
                                                                                         // ��� ������ ���������� ����������� �������������������
  virtual MbSolid       * MakeSolid( uint              & codeError,
    MakeSolidParam    & mksdParam,
    uint                shellThreadId ) override;
  // ����������� ����, ����������� ����
  virtual uint            PrepareInterimSolid( MbCurve3D            & curve,
    RPArray<MbFace>      & mbFaces,
    MbSNameMaker         & name,
    MbSolid              & prevSolid,
    MbBendValues         & params,        // ���� ��� �������������� ���������
    const IfComponent    * /*operOwner*/, // ��������� - �������� ���� ��������
    const IfComponent    * /*bodyOwner*/, // ��������� - �������� ����, ��� ������� ��� �������� �������������
    uint                   idShellThread,
    MbSolid              *&currSolid,
    WhatIsDone            wDone );
  // ����������� ����, ����������� ����
  virtual uint            PrepareLocalSolid( MbCurve3D            & curve,
    RPArray<MbFace>      & mbFaces,
    MbSNameMaker         & name,
    MbSolid              & prevSolid,
    MbBendValues         & params,        // ���� ��� �������������� ���������
    const IfComponent    * /*operOwner*/, // ��������� - �������� ���� ��������
    const IfComponent    * /*bodyOwner*/, // ��������� - �������� ����, ��� ������� ��� �������� �������������
    WhatIsDone             wDone,
    uint                   idShellThread,
    MbSolid              *&currSolid ) = 0;
  // ����������� ���� ���������
  virtual MbBendValues &  PrepareLocalParameters( const IfComponent * component ) = 0;

  // ���������� IfHotPoints
  /// ������� ���-�����
  virtual void GetHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool ) override;
  /// ������ �������������� ���-����� (�� ���-����� ������ ����� ������ ����)
  virtual bool BeginDragHotPoint( HotPointParam & hotPoint ) override;
  /// ���������� ��� �������������� ���-����� 
  // step - ��� ������������, ��� ����������� �������� � �������� ������������� �����
  virtual bool ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
    double step, D3Draw & pD3DrawTool ) override;
  /// ���������� �������������� ���-����� (�� ���-����� �������� ����� ������ ����)
  virtual bool EndDragHotPoint( HotPointParam & hotPoint, D3Draw & pD3DrawTool ) override;

  // ���������� ���-����� �������� ������ �� ShMtStraighten
  /// ������� ���-����� (��������������� ������� � 5000 �� 5999)
  virtual void GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ShMtBendObject &         children ) override;
  /// ���������� ��� �������������� ���-�����
  virtual bool ChangeChildrenHotPoint( HotPointParam &  hotPoint,
    const MbVector3D &     vector,
    double           step,
    ShMtBendObject & children ) override;

protected:
  virtual const AbsReferenceContainer * GetRefContainer( const IfComponent & bodyOwner ) const override { return &m_bendFaces; }

  /// ����������� �������������� ���-�����
  virtual void            UpdateHotPoints( HotPointBaseBendLineOperUtil & util,
    RPArray<HotPointParam> &       hotPoints,
    MbBendOverSegValues &          params,
    D3Draw &                       pD3DrawTool );
  /// ������� ������� ������� �� ���-������
  virtual void CreateDimensionsByHotPoints( IfUniqueName           * un,
    SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints ) override;
  /// �������� ��������� �������� ������� �� ���-������
  void UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util );
  /// ������������� ���������� ������� � ����������� ��������
  virtual void AssignDimensionsVariables( const SFDPArray<PartObject>  & dimensions ) override;

  void            CollectFaces( PArray<MbFace> & faces,
    const IfComponent & bodyOwner,
    uint                shellThreadId );
  void            SimplyCollectFaces( PArray<MbFace> & faces );


  /// �������� ���-����� �����������
  void            UpdateDirectionHotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util );
  /// �������� ���-����� ����������� �������
  void            UpdateDirection2HotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util );
  ///�������� ���-����� �������
  void            UpdateRadiusHotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util,
    MbBendOverSegValues & bendParameters );
  /// �������� ���-����� ����
  void            UpdateAngleHotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util,
    MbBendOverSegValues & bendParameters );

  bool            IsSavedUnhistoried( long version, ShMtBaseBendParameters::TypeRemoval typeRemoval ) const;

private:
  ShMtBaseBendLine& operator = ( const ShMtBaseBendLine & ); // �� �����������
                                                             /*
                                                             void ShMtBaseBendLine::Read( reader &in, ShMtBaseBendLine * obj );
                                                             void ShMtBaseBendLine::Write( writer &out, const ShMtBaseBendLine * obj );
                                                             */
  DECLARE_PERSISTENT_CLASS( ShMtBaseBendLine )
};


//------------------------------------------------------------------------------
// 
//---
ShMtBaseBendLine::ShMtBaseBendLine( IfUniqueName * un )
  : ShMtBaseBend( un ),
  m_bendFaces()
{
}


//------------------------------------------------------------------------------
// ����������� ������������
// ---
ShMtBaseBendLine::ShMtBaseBendLine( const ShMtBaseBendLine & other )
  : ShMtBaseBend( other ),
  m_bendFaces( other.m_bendFaces )
{
}


//------------------------------------------------------------------------------
//
// ---
ShMtBaseBendLine::ShMtBaseBendLine( TapeInit )
  : ShMtBaseBend( tapeInit ),
  m_bendFaces()
{
}

IMP_CLASS_DESC_FUNC( kompasDeprecatedAppID, ShMtBaseBendLine )
I_IMP_SOMETHING_FUNCS_AR( ShMtBaseBendLine )


I_IMP_QUERY_INTERFACE( ShMtBaseBendLine )
I_IMP_CASE_RI( ShMtBendLine );
I_IMP_CASE_RI( ShMtBend );
I_IMP_CASE_RI( HotPoints );
I_IMP_END_QI_FROM_BASE( ShMtBaseBend );


//------------------------------------------------------------------------------
// 
//---
bool ShMtBaseBendLine::IsThisParent( uint8 relType, const WrapSomething & parent ) const {
  return ShMtBaseBend::IsThisParent( rlt_Strong, parent ) || m_bendFaces.IsThisParent( rlt_Strong, parent );
}


//------------------------------------------------------------------------------
// ���������� ��������� �� ������� �������
//---
void ShMtBaseBendLine::Assign( const PrObject &other )
{
  ShMtBaseBend::Assign( other );

  const ShMtBaseBendLine * oper = TYPESAFE_DOWNCAST( &other, const ShMtBaseBendLine );
  if ( oper ) {
    SetParameters( oper->GetParameters() );
    m_bendFaces.Init( oper->m_bendFaces );
  }
}


//------------------------------------------------------------------------------
// �������� ��� ������� face ������� ����
// ---
bool ShMtBaseBendLine::SetResetBendFace( const WrapSomething & wrapper )
{
  bool res = false;
  ptrdiff_t ind = m_bendFaces.FindObject( wrapper );
  size_t cnt = m_bendFaces.Count();
  if ( ind > -1 && ind < cnt )
    m_bendFaces.DeleteInd( ind );
  else {
    m_bendFaces.AddObject( wrapper );
    res = true;                      // �������� ������
  }

  return res;
}


//---------------------------------------------------------------------------------------------
// ������� ��� face ������� ����
//---
void ShMtBaseBendLine::ResetAllBendFace()
{
  m_bendFaces.Reset();
  m_bendObjects.Flush();
}


//------------------------------------------------------------------------------
// �-�� faces ������� ����
// ---
size_t ShMtBaseBendLine::GetBendFaceCount() const
{
  return m_bendFaces.Count();
}


//-------------------------------------------------------------------------
// i - face ������� ����
//---
WrapSomething * ShMtBaseBendLine::GetBendFace( uint i )
{
  return const_cast<WrapSomething*>(const_cast<const ShMtBaseBendLine*>(this)->GetBendFace( i ));
}


//------------------------------------------------------------------------------
/**

*/
//---
const WrapSomething * ShMtBaseBendLine::GetBendFace( uint i ) const
{
  if ( i < GetBendFaceCount() )
    return &m_bendFaces.GetObjectTrans( i );
  return nullptr;
}


//------------------------------------------------------------------------------
/**

*/
//---
SArray<WrapSomething> ShMtBaseBendLine::GetBaseFaces() const
{
  SArray<WrapSomething> result;
  for ( size_t i = 0, c = GetBendFaceCount(); i < c; ++i )
    result.Add( m_bendFaces.GetObjectTrans( i ) );
  return result;
}


#ifdef MANY_THICKNESS_BODY_SHMT
//------------------------------------------------------------------------------
// ������� �� ��������� ������
// ---
bool ShMtBaseBendLine::IsSuitedObject( const WrapSomething & wrapper )
#else
bool ShMtBaseBendLine::IsSuitedObject( const WrapSomething & wrapper, double thickness )
#endif
{
  bool res = false;
  // �� K11 17.12.2008 ������ �37142: ��������� ����� ����� ������������ ���
  if ( wrapper && wrapper.component == wrapper.editComponent && !::IsMultiShellBody( wrapper ) )
  {
    IFPTR( AnyCurve ) anyCurve( wrapper.obj );
    if ( anyCurve )
      res = anyCurve->IsStraight();
    else {
      IFPTR( PartFace ) face( wrapper.obj );
      if ( face )
      {
        MbFace * mbFace = face->GetMathFace();
        if ( mbFace && mbFace->IsPlanar() && mbFace->GetName().IsSheet() )
        {
          // ���������� ����� ������ � ��� ������ ���� ��� ����������� ���� �� ���� ��� � ��������� ���
          // ��� ���� ��� ��� �����
          res = true;
          if ( m_bendFaces.Count() )
          {
            const WrapSomething & wr = m_bendFaces.GetObjectTrans( 0 );
            if ( wr )
            {
              IFPTR( Primitive ) primitive_0( wr.obj );
              IFPTR( Primitive ) primitiveWr( wrapper.obj );
              if ( primitive_0 && primitiveWr )
                res = primitive_0->GetShellThreadId() == primitiveWr->GetShellThreadId();
            }
          }
        }
      }
    }
  }

  return res;
}


//-------------------------------------------------------------------------------
// ������� ��� ������ faces
//---
void ShMtBaseBendLine::CollectFaces( PArray<MbFace>    & faces,
  const IfComponent & bodyOwner,
  uint                shellThreadId )
{
  if ( shellThreadId == -1 )
    SimplyCollectFaces( faces );
  else
  {
    faces.Flush();
    PArray< WrapSomething > wrappers( 0, 1, false );
    m_bendFaces.GetObjects( wrappers );
    DetailSolid * detailSolid = bodyOwner.ReturnDetailSolid( true );
    if ( detailSolid )
      for ( size_t i = 0, c = wrappers.Count(); i < c; i++ ) {
        IFPTR( PartFace ) pFace( wrappers[i]->obj );
        IFPTR( Primitive ) primitive( pFace );
        if ( pFace && primitive )
        {
          if ( detailSolid->GetFinalId( shellThreadId ) == detailSolid->GetFinalId( primitive->GetShellThreadId() ) )
          {
            MbFace * mbFace = pFace->GetMathFace();
            if ( mbFace )
              faces.Add( mbFace );
          }
        }
      }
  }
}


//-------------------------------------------------------------------------------
// ������� ��� ������ faces
//---
void ShMtBaseBendLine::SimplyCollectFaces( PArray<MbFace> & faces )
{
  faces.Flush();
  PArray< WrapSomething > wrappers( 0, 1, false );
  m_bendFaces.GetObjects( wrappers );
  for ( size_t i = 0, c = wrappers.Count(); i < c; i++ )
  {
    IFPTR( PartFace ) pFace( wrappers[i]->obj );
    if ( pFace )
    {
      MbFace * mbFace = pFace->GetMathFace();
      if ( mbFace )
        faces.Add( mbFace );
    }
  }
}


//------------------------------------------------------------------------------
// �������� ���������� ��� ��������������� Solida
// ---
MbSolid* ShMtBaseBendLine::MakeSolid( uint              & codeError,
  MakeSolidParam    & mksdParam,
  uint                shellThreadId )
{
  MbSolid * result = nullptr;

  // KVT K11 #38606
  if ( mksdParam.m_prevSolid && mksdParam.m_prevSolid->IsMultiSolid() )
    codeError = rt_MultiSolidDeflected;
  else
  {
    IFPTR( AnyCurve ) anyCurve( GetBendObject().obj );
    if ( mksdParam.m_prevSolid && anyCurve )
    {
      MbCurve3D * curve = anyCurve->GetMathCurve();
      if ( curve )
      {
        MbCurve3D * curveDup = nullptr;

        if ( anyCurve->IsFinite() )
          curveDup = curve;
        else
        {
          MbCartPoint3D pointBegin;
          MbCartPoint3D pointEnd;
          double t = curve->GetTMin();
          curve->PointOn( t, pointBegin );
          t = curve->GetTMax();
          curve->PointOn( t, pointEnd );
          curveDup = new MbLine3D( pointBegin, pointEnd );
        }
        curveDup->AddRef();

        // ����������� ���� ���������
        PartRebuildResult * partRResult = mksdParam.m_operOwner ? mksdParam.m_operOwner->ReturnPartRebuildResult() : nullptr;
        if ( partRResult )
        {
          PArray<MbFace> arrayFaces( 0, 1, false );
          CollectFaces( arrayFaces, *mksdParam.m_bodyOwner, shellThreadId );
          double thickness = 0.0;
          if ( arrayFaces.Count() &&
            CalcShMtThickness( *curveDup, arrayFaces, thickness ) )
          {
            SetThickness( thickness );
            MbBendValues & params = PrepareLocalParameters( mksdParam.m_operOwner );

            UtilBaseBendError bendErrors( GetParameters(), params, GetThickness() );
            SArray<uint> errors;
            bendErrors.GetErrors( errors );
            if ( errors.Count() == 0 ) {
              MbSNameMaker pComplexName( MainId(), MbSNameMaker::i_SideNone, 0/*mbFace->GetMainName()*/ );
              pComplexName.SetVersion( GetObjVersion() );

              codeError = PrepareInterimSolid( *curveDup,
                arrayFaces,
                pComplexName,
                *mksdParam.m_prevSolid,
                params,
                mksdParam.m_operOwner, // ��������� - �������� ���� ��������
                mksdParam.m_bodyOwner, // ��������� - �������� ����, ��� ������� ��� �������� �������������
                shellThreadId,
                result,
                mksdParam.m_wDone );

              if ( codeError != rt_Success && result )
              {
                ::DeleteItem( result );
                result = nullptr;
              }
            }
            delete &params;
          }
          else
            SetObjectErrorCode( *mksdParam.m_operOwner, -1, rt_NoIntersect );// KA V17 16.05.2016 KOMPAS-7873
        }

        ::ReleaseItem( curveDup );
      }
    }
  }
  return result;
}



//---------------------------------------------------------------------------------
// ����������� ����, �������������, ����������� ���� 
//---
uint ShMtBaseBendLine::PrepareInterimSolid( MbCurve3D            & curve,
  RPArray<MbFace>      & mbFaces,
  MbSNameMaker         & nameMaker,
  MbSolid              & prevSolid,
  MbBendValues         & params,    // ���� ��� �������������� ���������
  const IfComponent    * operOwner, // ��������� - �������� ���� ��������
  const IfComponent    * bodyOwner, // ��������� - �������� ����, ��� ������� ��� �������� �������������
  uint                   idShellThread,
  MbSolid              *&resultSolid,
  WhatIsDone            wDone )
{
  uint codeError = operOwner ? rt_Success : rt_ParameterError;

  if ( operOwner && mbFaces.Count() ) {
    MbPlacement3D place;
    mbFaces[0]->GetPlacement( &place );
    {
      UtilFindFixedPairFaces * utlFindFixedFaces = wDone == wd_Phantom ? nullptr : new UtilFindFixedPairFaces( mbFaces );  // ����� � ���������� ������������� ����� �����
      codeError = PrepareLocalSolid( curve,
        mbFaces,
        nameMaker,
        prevSolid,
        params,
        operOwner, // ��������� - �������� ���� ��������
        bodyOwner, // ��������� - �������� ����, ��� ������� ��� �������� �������������
        wDone,
        idShellThread,
        resultSolid );

      if ( utlFindFixedFaces ) {
        if ( resultSolid )
          utlFindFixedFaces->Run( *resultSolid, curve, IsLeftSideFix(), IsStraighten() && GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x07000121L ? this : nullptr );//-V595

        delete utlFindFixedFaces;
      }
    }

    if ( GetObjVersion().GetVersionContainer().GetAppVersion() > 0x0C000001L || GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0A000001L )
    {
      // �� K9 19.1.2007 ������ �20801 : ������ ����������� � ����������
      // CC K12 ����������� ���� ������ ����� ���������� ������� � �������������� ���� ������. ��� ���������� � 
      // CC K12 �������� #8 ���� �� ������. 
      // �� K12 ��������� ��� ����� � ������ ��� ������. � ���������� ����� ����� �������� ����� �������� ����� �� ��������.
      // �� K12 +����������� ������ 40270
      if ( IsStraighten() && resultSolid )
      {
        codeError = rt_SolidError;
        MbBendValues paramBends;           // ��������� ��������� ����
                                           // �� K12 			 paramBends.thickness = params.thickness;

        PArray<MbSheetMetalBend> bend( 0, 1, true );
        RPArray<MbFace> faces( 0, 1 );
        resultSolid->GetFaces( faces );
        for ( size_t i = 0, c = faces.Count(); i < c; i++ )
        {
          if ( faces[i]->GetMainName() == MainId() )
          {
            if ( faces[i]->GetName().IsInnerBend() )
            {
              ShMtBendObject * bendObj = FindParametrsBendByName( faces[i]->GetName() );
              if ( bendObj )
              {
                MbFace * innerFace = faces[i];
                PRECONDITION( innerFace != nullptr );
                MbFace * outerFace = ::FindOppositeFace( *innerFace, GetThickness()/* CC �12 params.thickness*/, st_CylinderSurface );
                if ( outerFace != nullptr )
                  bend.Add( new MbSheetMetalBend( innerFace, outerFace, bendObj->GetCoefficient(),
                    bendObj->GetAbsRadius(), 0.0/*angle*/, 0.0/*coneAngle*/ ) );
                else
                  return codeError;
              }
            }
          }
        }

        if ( bend.Count() )
        {
          // ����� ������������� �����
          MbFace *faceFix = nullptr;
          MbPlacement3D pl;
          // 1. �� ��������� ������� ������
          for ( size_t i = 0, c = faces.Count(); i < c; i++ )
          {
            if ( faces[i]->GetPlacement( &pl ) && pl.Complanar( place ) )
            {
              faceFix = faces[i];
              break;
            }
          }

          // 2. ������� ����� �������� ������ 
          if ( faceFix == nullptr )
          {
            for ( size_t k = 0, ck = bend.Count(); k < ck; k++ )
            {
              PRECONDITION( bend[k]->innerFaces.Count() == 1 );
              const MbFace * inFace = bend[k]->innerFaces[0];
              PRECONDITION( inFace != nullptr );
              MbLoop * loop = inFace->GetLoop( 0 );
              if ( loop )
              {
                RPArray<MbOrientedEdge> curves( 0, 1 );
                loop->GetOrientedEdges( curves );
                for ( size_t i = 0, c = curves.Count(); i < c; i++ )
                {
                  if ( curves[i]->IsStraight() ) {
                    MbFace * minFace = curves[i]->GetFaceMinus();
                    if ( minFace && !minFace->GetName().IsSheet() && minFace->GetSurface().GetSurface().IsA() == st_Plane )
                    {
                      faceFix = minFace;
                      break;
                    }
                  }
                }
                if ( faceFix )
                  break;
              }
            }
          }

          // 3. ����� ������������ ����� �������� ������
          if ( faceFix == nullptr )
          {
            for ( size_t i = 0, c = faces.Count(); i < c; i++ )
            {
              if ( faces[i]->GetPlacement( &pl ) )
              {
                if ( pl.GetAxisZ().Colinear( place.GetAxisZ() ) )
                {
                  faceFix = faces[i];
                  break;
                }
              }
            }
          }

          if ( faceFix )
          {
            paramBends.k = params.k;
            paramBends.radius = params.radius;
            paramBends.angle = params.angle;

            MbSolid * oldResult = resultSolid;
            const MbCartPoint fixedPoint;
            codeError = UnbendSheetSolid( *resultSolid,         // �������� ����
              cm_KeepHistory,  // �� �������� � ������ �������� ��������� ����
              bend,            // ������ ������, ��������� �� ��� ������ - ���������� � ������� ������ �����
              *faceFix,        // �����, ���������� �����������
              fixedPoint,      // ����� � ��������������� ������� faceFix, � ������� ���������� ����� �������� ��������
                               // CC K12 																				params,          // ��������� ��������� ����
              nameMaker,            // �����������
              resultSolid );        // �������������� ����
            if ( resultSolid != oldResult )
              ::DeleteItem( oldResult );
          }
        }
      }
    }
  }

  return codeError;
}
// ��� K7+ #endif


//------------------------------------------------------------------------------
// �������� �������������� ��������
// ---
bool ShMtBaseBendLine::IsValid() const
{
  bool res = ShMtBaseBend::IsValid();
  if ( res )
    res = m_bendFaces.IsFull();

  return res;
}


//------------------------------------------------------------------------------
// ������������ �����
// ---
bool ShMtBaseBendLine::RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath )
{
  bool res = ShMtBaseBend::RestoreReferences( result, allRefs, withMath );
  bool resObj = m_bendFaces.RestoreReferences( result, allRefs, withMath );

  if ( IsJustRead() && GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0B000001L )
  {
    // ��� ������ ������ 11 ���� ���������� �� ����� � ��������
    IFPTR( Primitive ) prim( m_bendFaces.GetObjectTrans( 0 ).obj );
    if ( prim )
      for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
        m_bendObjects[i]->m_extraName = prim->GetShellThreadId();
  }

  return res && resObj;
} // RestoreReferences


  //------------------------------------------------------------------------------
  // �������� ������� ������� soft = true - ������ ������� (�� ������ �����)
  // ---
void ShMtBaseBendLine::ClearReferencedObjects( bool soft )
{
  ShMtBaseBend::ClearReferencedObjects( soft );
  m_bendFaces.ClearObjects( soft );
}


//------------------------------------------------------------------------------
// �������� ������ �� �������
// ---
bool ShMtBaseBendLine::ChangeReferences( ChangeReferencesData & changedRefs )
{
  bool res = ShMtBaseBend::ChangeReferences( changedRefs );
  if ( m_bendFaces.ChangeReferences( changedRefs ) )
    res = true;

  return res;
}


//------------------------------------------------------------------------------
// �������� ������ �������������� �� �������
// ---
void ShMtBaseBendLine::WhatsWrong( WarningsArray & warnings ) {
  ShMtBaseBend::WhatsWrong( warnings );

  for ( size_t i = warnings.Count(); i--; )
  {
    if ( warnings[i]->GetResId() == IDP_RT_NOINTERSECT )// KA V17 16.05.2016 KOMPAS-7873 ��� �� �����������, ������� ��� ���� ��������
      warnings.AddWarning( IDP_SF_ERROR, module );
  }

  if ( !m_bendFaces.IsFull() )
    warnings.AddWarning( IDP_WARNING_LOSE_SUPPORT_OBJ, module ); // "������� ���� ��� ��������� ������� ��������"
}


//------------------------------------------------------------------------------
// ������ ������ �����
// ---
void ShMtBaseBendLine::SetBendObject( const WrapSomething * wrapper ) {
  ShMtBaseBend::SetObject( wrapper );
}


//------------------------------------------------------------------------------
// �������� ������ �����
// ---
const WrapSomething & ShMtBaseBendLine::GetBendObject() const {
  return ShMtBaseBend::GetObject();
}


//-----------------------------------------------------------------------------
//������������� �� ����������� � ������
//---
bool ShMtBaseBendLine::PrepareToAdding( PartRebuildResult & result, PartObject * toObj )
{
  if ( ShMtBaseBend::PrepareToAdding( result, toObj ) )
  {
    if ( !GetFlagValue( O3D_IsReading ) ) // �� �������� �� ������, � ������ ��� new  BUG 68896
    {
      GetParameters().PrepareToAdding( result, this );
      UpdateMeasurePars( gtt_none, true );
    }
    return true;
  }

  return false;
}


//------------------------------------------------------------------------------
// ������ ������ ����������, ��������� � �����������
// ---
void ShMtBaseBendLine::GetVariableParameters( VarParArray & parVars, ParCollType parCollType )
{
  ::ShMtSetAsInformation( GetParameters().m_radius, ShMtManager::sm_radius, m_bendObjects.Count() > 0 && !IsAnySubBendByOwner() );

  ShMtBaseBend::GetVariableParameters( parVars, parCollType );
  GetParameters().GetVariableParameters( parVars, parCollType == all_pars, true );
}



//-----------------------------------------------------------------------------
// �� K6+ ������ ��� ����������, ��������� � �����������
//---
void ShMtBaseBendLine::ForgetVariables() {
  ShMtBaseBend::ForgetVariables();
  GetParameters().ForgetVariables();
}


// *** ���-����� ***************************************************************
//------------------------------------------------------------------------------
// ������� ���-�����
// ---
void ShMtBaseBendLine::GetHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  hotPoints.Add( new HotPointDirectionParam( ehp_ShMtBendDirection, editComponent, topComponent ) ); // �����������
  hotPoints.Add( new HotPointDirectionParam( ehp_ShMtBendDirection2, editComponent, topComponent ) ); // ����������� �������
  hotPoints.Add( new HotPointBendLineRadiusParam( ehp_ShMtBendRadius, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ������ ����� 
}


//-------------------------------------------------------------------------------
// ������ �������������� ���-����� (�� ���-����� ������ ����� ������ ����)
// ---
bool ShMtBaseBendLine::BeginDragHotPoint( HotPointParam & hotPoint )
{
  /* CC K12
  if ( hotPoint.GetIdent() == ehp_ShMtBendAngle &&
  !((HotPointBendAngleParam &)hotPoint).GetCircle() )
  return true;
  */
  return false;
}


//-----------------------------------------------------------------------------
// ���������� �������������� ���-����� (�� ���-����� �������� ����� ������ ����)
//---
bool ShMtBaseBendLine::EndDragHotPoint( HotPointParam & hotPoint, D3Draw & pD3DrawTool )
{
  // CC K12  return hotPoint.GetIdent() == ehp_ShMtBendAngle;
  return false;
}


//-----------------------------------------------------------------------------
// ���������� ��� �������������� ���-����� 
/**
\param hotPoint - ��������� ���-�����
\param vector - ������ ������
\param step - ��� ������������, ��� ����������� �������� � �������� ������������� �����
*/
//---
bool ShMtBaseBendLine::ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
  double step, D3Draw & pD3DrawTool )
{
  bool ret = false;

  switch ( hotPoint.GetIdent() )
  {
  case ehp_ShMtBendDirection: // �����������
    ret = ((HotPointDirectionParam &)hotPoint).SetHotPoint( vector, GetParameters().m_dirAngle, pD3DrawTool );
    break;

  case ehp_ShMtBendDirection2: // ����������� �������
    ret = ((HotPointDirectionParam &)hotPoint).SetHotPoint( vector,
      ((ShMtBendLineParameters &)GetParameters()).m_leftFixed,
      pD3DrawTool );
    break;

  case ehp_ShMtBendRadius: // ������ �����
    ret = ((HotPointBendLineRadiusParam &)hotPoint).SetRadiusHotPoint( vector, step,
      GetParameters(),
      GetParameters(),
      GetThickness() );
    break;

  case ehp_ShMtBendAngle: // ���� �����
    ret = ::SetHotPointAnglePar( (HotPointBendAngleParam&)hotPoint, vector,
      step, GetParameters() );
    break;
  }

  return ret;
}


//------------------------------------------------------------------------------
/**
�������� ���-����� �����������
\param hotPoints - ������ ���-����� ��������
\param util - ������� ��� ������� ���-�����
*/
//--- 
void ShMtBaseBendLine::UpdateDirectionHotPoint( RPArray<HotPointParam> & hotPoints,
  HotPointBaseBendLineOperUtil & util )
{
  HotPointParam * directionHotPoint = FindHotPointWithId( ehp_ShMtBendDirection, hotPoints );

  if ( directionHotPoint != nullptr )
    util.UpdateDirctionHotPoint( *directionHotPoint, GetParameters() );
}


//------------------------------------------------------------------------------
/**
�������� ���-����� ����������� �������
\param hotPoints - ������ ���-�����
\param util - ������� ��� ������� ���-�����
*/
//--- 
void ShMtBaseBendLine::UpdateDirection2HotPoint( RPArray<HotPointParam> & hotPoints,
  HotPointBaseBendLineOperUtil & util )
{
  HotPointParam * direction2HotPoint = FindHotPointWithId( ehp_ShMtBendDirection2, hotPoints );

  if ( direction2HotPoint != nullptr )
    util.UpdateFixedSidHotPoint( *direction2HotPoint );
}


//------------------------------------------------------------------------------
/**
�������� ���-����� �������
\param hotPoints - ������ ���-�����
\param util - ������� ��� ������� ���-�����
\param bendParameters - ��������� �����
*/
//--- 
void ShMtBaseBendLine::UpdateRadiusHotPoint( RPArray<HotPointParam> & hotPoints,
  HotPointBaseBendLineOperUtil & util,
  MbBendOverSegValues & bendParameters )
{
  HotPointBendLineRadiusParam * radiusHotPoint = dynamic_cast<HotPointBendLineRadiusParam *>(FindHotPointWithId( ehp_ShMtBendRadius, hotPoints ));

  if ( radiusHotPoint != nullptr )
  {
    if ( m_bendObjects.Count() > 0 && m_bendObjects[0]->m_byOwner )
    {
      radiusHotPoint->params = bendParameters;
      radiusHotPoint->m_deepness = bendParameters.displacement;

      util.UpdateRadiusHotPoint( *radiusHotPoint, bendParameters, (ShMtBendLineParameters &)GetParameters() );
    }
    else
      radiusHotPoint->SetBasePoint( nullptr );
  }
}


//------------------------------------------------------------------------------
/**
�������� ���-����� ����
\param hotPoints - ������ ���-�����
\param util - ������� ��� ������� ���-�����
\param bendParameters - ��������� �����
*/
//--- 
void ShMtBaseBendLine::UpdateAngleHotPoint( RPArray<HotPointParam> & hotPoints,
  HotPointBaseBendLineOperUtil & util,
  MbBendOverSegValues & bendParameters )
{
  HotPointBendAngleParam * angleHotPoint = dynamic_cast<HotPointBendAngleParam *>(FindHotPointWithId( ehp_ShMtBendAngle, hotPoints ));

  if ( angleHotPoint != nullptr )
  {
    angleHotPoint->params = bendParameters;
    util.UpdateAngleHotPoint( *angleHotPoint, bendParameters, GetParameters() );
  }
}


//-----------------------------------------------------------------------------
// ����������� �������������� ���-�����
//---
void ShMtBaseBendLine::UpdateHotPoints( HotPointBaseBendLineOperUtil & util,
  RPArray<HotPointParam> &       hotPoints,
  MbBendOverSegValues &          params,
  D3Draw &                       pD3DrawTool )
{
  UpdateDirectionHotPoint( hotPoints, util );
  UpdateDirection2HotPoint( hotPoints, util );
  UpdateRadiusHotPoint( hotPoints, util, params );
  UpdateAngleHotPoint( hotPoints, util, params );
}




//------------------------------------------------------------------------------
/**
������� ������� ������� �� ���-������
\param  un         - ���������� ���
\param  dimensions - �������
\param  hotPoints  - ���-�����
\return ���
*/
//---
void ShMtBaseBendLine::CreateDimensionsByHotPoints( IfUniqueName           * un,
  SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
  const RPArray<HotPointParam> & hotPoints )
{
  for ( size_t i = 0, c = hotPoints.Count(); i < c; ++i )
  {
    if ( HotPointParam * par = hotPoints[i] )
    {
      int hpID = par->GetIdent();
      switch ( hpID )
      {
      case ehp_ShMtBendRadius:
        ::AttachGnrRadialDimension( dimensions, *par, hpID );
        break;
      case ehp_ShMtBendAngle:
        ::AttachGnrAngularDimension( dimensions, *par, hpID );
        break;
      }
    }
  }

  ShMtBaseBend::CreateDimensionsByHotPoints( un, dimensions, hotPoints );
}


//------------------------------------------------------------------------------
/**
�������� ��������� �������� ������� �� ���-������
\param  dimensions - �������
\param  hotPoints  - ���-�����
\return ���
*/
//---
void ShMtBaseBendLine::UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  HotPointBaseBendLineOperUtil & util )
{
  if ( !IsValid() )
    return;

  if ( ShMtBendLineParameters * pars = dynamic_cast<ShMtBendLineParameters*>(&GetParameters()) )
  {
    IFPTR( OperationCircularDimension ) dimRad( ::FindGnrDimension( dimensions, ehp_ShMtBendRadius, false ) );
    if ( dimRad )
      util.UpdateBendRadDimension( *dimRad, pars->m_radius );

    //������� ������
    IFPTR( OperationAngularDimension ) anglDim( ::FindGnrDimension( dimensions, ehp_ShMtBendAngle, false ) );
    if ( anglDim )
      util.UpdateAngularBendDim( *anglDim, *pars );
  }
}


//------------------------------------------------------------------------------
/**
������������� ���������� ������� � ����������� ��������
\param  dimensions - �������
\return ���
*/
//---
void ShMtBaseBendLine::AssignDimensionsVariables( const SFDPArray<PartObject> & dimensions )
{
  if ( ShMtBendLineParameters * pars = dynamic_cast<ShMtBendLineParameters*>(&GetParameters()) )
    ::UpdateDimensionVar( dimensions, ehp_ShMtBendRadius, pars->m_radius );
}


//------------------------------------------------------------------------------
// ������� ���-����� (��������������� ������� � 5000 �� 5999)
// ---
void ShMtBaseBendLine::GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ShMtBendObject &         children )
{
  hotPoints.Add( new HotPointBendLineRadiusParam( ehp_ShMtBendObjectRadius, editComponent,
    topComponent ) ); // ������ ����� 
}


//-----------------------------------------------------------------------------
// ���������� ��� �������������� ���-����� 
/**
\param hotPoint - ��������������� ���-�����
\param vector - ������ ��������
\param step - ��� ������������, ��� ����������� �������� � �������� ������������� �����
\param children - ����, ��� �������� ���������� ���-�����
*/
//---
bool ShMtBaseBendLine::ChangeChildrenHotPoint( HotPointParam &  hotPoint,
  const MbVector3D &     vector,
  double           step,
  ShMtBendObject & children )
{
  bool ret = false;

  if ( hotPoint.GetIdent() == ehp_ShMtBendObjectRadius ) // ������ �����
    ret = ((HotPointBendLineRadiusParam &)hotPoint).SetRadiusHotPoint( vector, step, GetParameters(), children, GetThickness() );

  return ret;
}
// *** ���-����� ***************************************************************


//--------------------------------------------------------------------------------
/**
���������� �� ���������� ��� �������
\param version
\param typeRemoval
\return bool
*/
//---
bool ShMtBaseBendLine::IsSavedUnhistoried( long version, ShMtBaseBendParameters::TypeRemoval typeRemoval ) const
{               // V18 (KOMPAS-23638)
  return version < 0x12000001L && typeRemoval == ShMtBaseBendParameters::tr_byCentre;
}


//------------------------------------------------------------------------------
// ������ �� ������
// ---
void ShMtBaseBendLine::Read( reader &in, ShMtBaseBendLine * obj ) {
  ReadBase( in, (ShMtBaseBend *)obj );
  in >> obj->m_bendFaces;
}


//------------------------------------------------------------------------------
// ������ � �����
// ---
void ShMtBaseBendLine::Write( writer &out, const ShMtBaseBendLine * obj ) {
  if ( out.AppVersion() < 0x06000033L )
    out.setState( io::cantWriteObject ); // ���������� ��������� ��� ��������� ��������
  else {
    WriteBase( out, (const ShMtBaseBend *)obj );
    out << obj->m_bendFaces;
  }
}


////////////////////////////////////////////////////////////////////////////////
//
// ������ �������� ���� �� �����
//
////////////////////////////////////////////////////////////////////////////////
class ShMtBendLine : public ShMtBaseBendLine,
  public IfCopibleObject,
  public IfCopibleOperation
{
public:
  ShMtBendLineParameters  m_params;

public:
  // �����������, ������ ���������� ��������� ������ �� �����������, ����� ����� �����
  // ����������� ��� ������������� � ������������ ��������
  ShMtBendLine( const ShMtBendLineParameters &, IfUniqueName * un );
  // ����������� ������������
  ShMtBendLine( const ShMtBendLine & );

public:
  I_DECLARE_SOMETHING_FUNCS;

  virtual double                            GetOwnerAbsAngle() const;
  virtual double                            GetOwnerValueBend() const;
  virtual double                            GetOwnerAbsRadius() const;  // ���������� ������ �����
  virtual bool                              IsOwnerInternalRad() const;  // �� ����������� �������?
  virtual UnfoldType                        GetOwnerTypeUnfold() const;  // ���� ��� ���������  
  virtual void                              SetOwnerTypeUnfold( UnfoldType t );  // ������ ��� ���������  
  virtual bool                              GetOwnerFlagByBase() const;  // 
  virtual void                              SetOwnerFlagByBase( bool val );        // 
  virtual double                            GetOwnerCoefficient() const;  // �����������
  virtual bool                              IfOwnerBaseChanged( IfComponent * trans );        // ���� ���������� ����� ���� ���������, � ���� ������ ���� �� ����, �� ��������� ���� 
  virtual double                            GetThickness() const;  // ���� �������
  virtual void                              SetThickness( double );// ������ �������      
                                                                   // ����� ��� ��� ����������� ��������������
  virtual void                              EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp );

  virtual void                              SetParameters( const ShMtBaseBendParameters & );
  virtual ShMtBaseBendParameters          & GetParameters() const;

  virtual bool                              IsLeftSideFix();
  //---------------------------------------------------------------------------
  // IfCopibleObject
  // ������� ����� �������
  virtual IfPartObjectCopy * CreateObjectCopy( IfComponent & compOwner ) const;
  // ������� ���������� �� ��������� ����������
  virtual bool               NeedCopyOnComp( const IfComponent & component ) const;
  virtual bool               IsAuxGeom() const { return false; } ///< ��� ��������������� ���������?
                                                                 /// ����� �� �������� ������� �� ���������� � �������
  virtual bool               CanCopyByVarsTable() const { return false; }

  //---------------------------------------------------------------------------
  // IfCopibleOperation
  // ����� �� ���������� ��� �������� ���-������ �����, ����� ��� �������������?
  // �.�. ����� MakePureSolid ��� ����� ���������� IfCopibleOperationOnPlaces,
  // IfCopibleOperationOnEdges � �.�.
  virtual bool            CanCopyNonGeometrically() const;

  //------------------------------------------------------------------------------
  // ����� ������� ������� ������ 
  virtual uint            GetObjType() const;                            // ������� ��� �������
  virtual uint            GetObjClass() const;                            // ������� ����� ��� �������
  virtual uint            OperationSubType() const; // ������� ������ ��������
  virtual PrObject      & Duplicate() const;

  virtual void            PrepareToBuild( const IfComponent & ); ///< ������������� � ����������
  virtual void            PostBuild(); ///< ����� �������� ����� ����������
                                       /// ����������� ����, ����������� ����
  virtual uint            PrepareLocalSolid( MbCurve3D            & curve,
    RPArray<MbFace>      & mbFaces,
    MbSNameMaker         & nameMaker,
    MbSolid              & prevSolid,
    MbBendValues         & params,        // ���� ��� �������������� ���������
    const IfComponent    * /*operOwner*/, // ��������� - �������� ���� ��������
    const IfComponent    * /*bodyOwner*/, // ��������� - �������� ����, ��� ������� ��� �������� �������������
    WhatIsDone             wDone,
    uint                   idShellThread,
    MbSolid              *&currSolid );
  // ����������� ���� ���������
  virtual MbBendValues &  PrepareLocalParameters( const IfComponent * component );

  // �������� ������ �������������� �� �������
  virtual void            WhatsWrong( WarningsArray & warnings );
  virtual bool            IsValid() const;  // �������� �������������� �������

                                            /// �������� �� ��� ��������� �������� ������
  virtual bool            IsCanSetTolerance( const VarParameter & varParameter ) const;
  /// ��� ��������� �������� ����������� ���������� ������
  virtual double          GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const;
  /// �������� �� �������� �������
  virtual bool            ParameterIsAngular( const VarParameter & varParameter ) const;
  virtual bool            CopyInnerProps( const IfPartObject& source,
    uint sourceSubObjectIndex,
    uint destSubObjectIndex ) override;
  /// ���� �� ��������� ��� ������ ��� �������
  virtual bool            IsNeedSaveAsUnhistored( long version ) const override;

  // ���������� IfHotPoints
  /// ����������� �������������� ���-�����
  virtual void UpdateHotPoints( RPArray<HotPointParam> & hotPoints,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool );
  /// ������� ���-�����               
  virtual void GetHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool );
  /// ����������� �������������� ���-�����
  virtual void UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    ShMtBendObject &         children ) override;
  /// ����� �� � ������� ���� ��������� ����������� �������
  virtual bool IsDimensionsAllowed() const;
  /// �������� ��������� �������� ������� �� ���-������
  virtual void UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    const PArray<OperationSpecificDispositionVariant>* pVariants );
  /// ������������� ���������� ������� � ����������� ��������
  virtual void AssignDimensionsVariables( const SFDPArray<PartObject>  & dimensions );
  /// ������� ������� ������� �� ���-������ ��� �������� ��������
  virtual void CreateChildrenDimensionsByHotPoints( IfUniqueName           * un,
    SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children );
  /// �������� ��������� �������� ������� �� ���-������ ��� �������� ��������
  virtual void UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject         & children );
  /// ������������� ������� �������� �������� - ������
  virtual void AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
    ShMtBendObject        & children );

  /// �������� ���������� ��� ���������
  virtual void              RenameUnique( IfUniqueName & un );

  /// ������ ���� ��� ��������� ������� ���������� �� �������
  virtual void UpdateMeasurePars( GeneralToleranceType gType, bool recalc );

private:
  std::unique_ptr<HotPointBendLineOperUtil> GetHotPointUtil( MbBendOverSegValues *& params,
    IfComponent & editComponent,
    ShMtBendObject * children );

private:
  void operator = ( const ShMtBendLine & ); // �� �����������

                                            //  DECLARE_PERSISTENT_CLASS( ShMtBendLine )
  DECLARE_PERSISTENT_CLASS( ShMtBendLine )
};



//------------------------------------------------------------------------------
// �����������, ������ ���������� ��������� ������ �� �����������, ����� ����� �����
// ����������� ��� ������������� � ������������ ��������
ShMtBendLine::ShMtBendLine( const ShMtBendLineParameters & _pars, IfUniqueName * un )
  : ShMtBaseBendLine( un ),
  m_params( _pars )
{
  if ( un )
    m_params.RenameUnique( *un );
}


//------------------------------------------------------------------------------
// ����������� ������������
// ---
ShMtBendLine::ShMtBendLine( const ShMtBendLine & other )
  : ShMtBaseBendLine( other ),
  m_params( other.m_params )
{
}


//------------------------------------------------------------------------------
//
// ---
ShMtBendLine::ShMtBendLine( TapeInit )
  : ShMtBaseBendLine( tapeInit ),
  m_params( nullptr ) // ���������
{
}


I_IMP_SOMETHING_FUNCS_AR( ShMtBendLine )


//------------------------------------------------------------------------------
// ������ �� ��������� ����������
// ---
// IfSomething * ShMtBendLine::QueryInterface ( uint iid ) {
//   return ShMtBaseBendLine::QueryInterface( iid );
// }
I_IMP_QUERY_INTERFACE( ShMtBendLine )
I_IMP_CASE_RI( CopibleObject );
I_IMP_CASE_RI( CopibleOperation );
I_IMP_END_QI_FROM_BASE( ShMtBaseBendLine );


//------------------------------------------------------------------------------
// ������� ����� ������� - ����� ��������� ����������� ������
// ---
IfPartObjectCopy * ShMtBendLine::CreateObjectCopy( IfComponent & compOwner ) const
{
  return ::CreateObjectCopy<PartObject, PartObjectCopy>( compOwner, *this );
}


//------------------------------------------------------------------------------
// ������� ���������� �� ��������� ����������
// ---
bool ShMtBendLine::NeedCopyOnComp( const IfComponent & component ) const
{
  return !!GetPartOperationResult().GetResult( component );
}


//------------------------------------------------------------------------------
// ����� �� ���������� ��� �������� ���-������ �����, ����� ��� �������������?
// �.�. ����� MakePureSolid ��� ����� ���������� IfCopibleOperationOnPlaces,
// IfCopibleOperationOnEdges � �.�.
// ---
bool ShMtBendLine::CanCopyNonGeometrically() const
{
  // ���� ������ �������������.
  return false;
}


uint ShMtBendLine::GetObjType()  const { return ot_ShMtBendLine; } // ������� ��� �������
uint ShMtBendLine::GetObjClass() const { return oc_ShMtBendLine; } // ������� ����� ��� �������
uint ShMtBendLine::OperationSubType() const { return bo_Union; }          // ������� ������ ��������

                                                                          //-----------------------------------------------------------------------------
                                                                          //
                                                                          //---
PrObject & ShMtBendLine::Duplicate() const
{
  return *new ShMtBendLine( *this );
}


//------------------------------------------------------------------------------
// ������ ���������
// ---
void ShMtBendLine::SetParameters( const ShMtBaseBendParameters & p ) {
  m_params = (ShMtBendLineParameters&)p;
}


//------------------------------------------------------------------------------
// �������� ���������
// ---
ShMtBaseBendParameters & ShMtBendLine::GetParameters() const {
  return const_cast<ShMtBendLineParameters&>(m_params);
}


//------------------------------------------------------------------------------
// ����� ��� ��� ����������� ��������������
// ---
void ShMtBendLine::EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp ) {
  IFPTR( ShMtBendLine ) ini( const_cast<IfSheetMetalOperation*>(&iniObj) );
  if ( ini ) {
    m_params.EditingEnd( ini->GetParameters(), *editComp.ReturnPartRebuildResult(), this );
  }
}


//------------------------------------------------------------------------------
// �������� �������������� ��������
// ---
bool ShMtBendLine::IsValid() const {
  bool res = ShMtBaseBendLine::IsValid();
  if ( res )
    res = m_params.IsValidParam();

  return res;
}


//------------------------------------------------------------------------------
// �������� ������ �������������� �� �������
// ---
void ShMtBendLine::WhatsWrong( WarningsArray & warnings ) {
  ShMtBaseBendLine::WhatsWrong( warnings );

  MbBendOverSegValues params;
  m_params.GetValues( params );
  UtilBaseBendError errors( m_params, params, GetThickness() );
  errors.WhatsWrong( warnings );

  m_params.WhatsWrongParam( warnings );
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendLine::GetThickness() const {
  return m_params.m_thickness;    // �������
}


//---------------------------------------------------------------------------------
//
//---
void ShMtBendLine::SetThickness( double h ) {
  m_params.m_thickness = h;    // �������
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendLine::GetOwnerAbsRadius() const {
  return m_params.m_internal ? m_params.GetRadius() : (m_params.GetRadius() - GetThickness());    // ���������� ������ �����
}


//---------------------------------------------------------------------------------
// �����������, ������������ ��������� ������������ ����
//---
double ShMtBendLine::GetOwnerValueBend() const {
  double val;
  m_params.GetValueBend( val );
  return val;
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendLine::GetOwnerAbsAngle() const {
  return  m_params.GetAbsAngle();     // ���� ����� 
}


//---------------------------------------------------------------------------------
// �� ����������� �������  
//---
bool ShMtBendLine::IsOwnerInternalRad() const {
  return m_params.m_internal;
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendLine::GetOwnerCoefficient() const {
  return m_params.GetCoefficient();
}


//---------------------------------------------------------------------------------
// ���� ���������� ����� ���� ���������, � ���� ������ ���� �� ����, �� ��������� ���� 
//---
bool ShMtBendLine::IfOwnerBaseChanged( IfComponent * trans ) {
  return m_params.ReadBaseIfChanged( trans );
}


//---------------------------------------------------------------------------------
// ���� ��� ���������  
//---
UnfoldType ShMtBendLine::GetOwnerTypeUnfold() const {
  return m_params.m_typeUnfold;
}


//---------------------------------------------------------------------------------
// ������ ��� ���������  
//---
void ShMtBendLine::SetOwnerTypeUnfold( UnfoldType t ) {
  m_params.m_typeUnfold = t;
}


//---------------------------------------------------------------------------------
//
//---
void ShMtBendLine::SetOwnerFlagByBase( bool val ) {
  m_params.SetFlagUnfoldByBase( val );
}


//---------------------------------------------------------------------------------
//
//---
bool ShMtBendLine::GetOwnerFlagByBase() const {
  return m_params.GetFlagUnfoldByBase();
}


//------------------------------------------------------------------------------
// ����������� ���� ���������
// ---
MbBendValues & ShMtBendLine::PrepareLocalParameters( const IfComponent * component ) {
  MbBendOverSegValues * params = new MbBendOverSegValues();
  IfComponent * comp = const_cast<IfComponent*>(component);
  m_params.ReadBaseIfChanged( comp ); // ���� ��������� ��������� ����� �� ����, �� ���������� ����, ���� ����
  m_params.GetValues( *params );

  // CC K8 � ������ ������� ���������� �������� ������������ �����
  if ( GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0800000BL )
    m_params.CalcAfterValues( *params );

  return *params;
}


//------------------------------------------------------------------------------
// ������������� � ����������
//---
void ShMtBendLine::PrepareToBuild( const IfComponent & operOwner ) // ��������� - �������� ���� ��������
{
  creatorBends = new CreatorForBendUnfoldParams( *this, (IfComponent*)&operOwner );
}


//------------------------------------------------------------------------------
// ����� �������� ����� ����������
//---
void ShMtBendLine::PostBuild()
{
  delete creatorBends;
  creatorBends = nullptr;
}


//---------------------------------------------------------------------------------
// ����������� ����, ����������� ����
//---
uint ShMtBendLine::PrepareLocalSolid( MbCurve3D            & curve,
  RPArray<MbFace>      & mbFaces,
  MbSNameMaker         & nameMaker,
  MbSolid              & prevSolid,
  MbBendValues         & params,    // ���� ��� �������������� ���������
  const IfComponent    * operOwner, // ��������� - �������� ���� ��������
  const IfComponent    * bodyOwner, // ��������� - �������� ����, ��� ������� ��� �������� �������������
  WhatIsDone             wDone,
  uint                   idShellThread,
  MbSolid              *&result )
{
  MbResultType codeError = operOwner ? rt_Success : rt_ParameterError;

  if ( operOwner && mbFaces.Count() ) {
    PartRebuildResult * partRResult = operOwner->ReturnPartRebuildResult();
    if ( partRResult )
    {
      if ( creatorBends )
      {
        creatorBends->Build( 1/*count*/, idShellThread );
        creatorBends->m_prepare = wDone != wd_Phantom;
        creatorBends->GetParams( params, m_params.m_dirAngle );
      }
      // CC K8 ��� ����� ������ ���������� �������� ���� ����������� � ����� �����
      if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
        m_params.CalcAfterValues( (MbBendOverSegValues&)params );

      if ( IsValidParamBaseTable() ) {
        codeError = ::BendSheetSolidOverSegment( prevSolid,                       // �������� ����
          wDone == wd_Phantom ? cm_Copy :
          cm_KeepHistory,                  // �� �������� � ������ �������� ��������� ����
          mbFaces,                         // ���������� �����
          curve,                           // �����, � ������� ����� �������
                                           /*����������� ������ 40270*/
          GetObjVersion().GetVersionContainer().GetAppVersion() > 0x0C000001L || GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0A000001L ? false : IsStraighten(),                  // �����
          (MbBendOverSegValues&)params,    // ��������� ��������� ����
          nameMaker,                            // �����������
          result );                        // �������������� ����
                                           // ���� ��������� ��� ����� � �� �������
        if ( creatorBends && wDone != wd_Phantom )
          creatorBends->SetResult( result );
      }
    }
  }

  return codeError;
}


//------------------------------------------------------------------------------
// �������� �� ��� ��������� �������� ������
// ---
bool ShMtBendLine::IsCanSetTolerance( const VarParameter & varParameter ) const
{
  if ( IsSubBendsParameter( varParameter ) )
  {
    return IsCanSetToleranceOnSubBendsParameter( varParameter );
  }
  else
  {
    if ( m_params.IsContainsVarParameter( varParameter ) )
      return m_params.IsCanSetTolerance( varParameter );
    else
      return ShMtBaseBendLine::IsCanSetTolerance( varParameter );
  }
}


//------------------------------------------------------------------------------
// �������� �� �������� �������
// ---
bool ShMtBendLine::ParameterIsAngular( const VarParameter & varParameter ) const
{
  if ( &varParameter == &const_cast<ShMtBendLineParameters&>(m_params).GetAngleVar() )
    return  true;

  return false;
}


//------------------------------------------------------------------------------
//
// ---
bool ShMtBendLine::CopyInnerProps( const IfPartObject& source,
  uint sourceSubObjectIndex,
  uint destSubObjectIndex )
{
  bool result = ShMtBaseBendLine::CopyStyleInnerProps( source );

  if ( const ShMtBendLine* pSource = dynamic_cast<const ShMtBendLine*>(&source) ) {
    m_params = pSource->m_params;
  }

  return result;
}


//------------------------------------------------------------------------------
// ��� ��������� �������� ����������� ���������� ������
// ---
double ShMtBendLine::GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const
{
  if ( &varParameter == &const_cast<ShMtBendLineParameters&>(m_params).GetAngleVar() )
  {
    //    double angle = m_params.m_typeAngle ? m_params.GetAngle() : 180 - m_params.GetAngle();
    // CC K14 ��� ��������� ������� ���������� ������. ����������� � ����������
    return reader.GetTolerance( m_params.m_distance, // ������� ������ BUG 62631
      vd_angle,
      tType );
  }
  return ShMtBaseBend::GetGeneralTolerance( varParameter, tType, reader );
}


//------------------------------------------------------------------------------------
/// ������ ���� ��� ��������� ������� ���������� �� �������
/**
// \method    InitGeneralTolerances
// \access    public
// \return    void
// \qualifier
*/
//---
void ShMtBendLine::UpdateMeasurePars( GeneralToleranceType gType, bool recalc )
{
  if ( recalc )
  {
    // � ������ ����� ���������� ������
    double rad = MB_MAXDOUBLE;
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
    {
      double rad_i = m_bendObjects[i]->GetAbsRadius();
      if ( rad_i < rad )
        rad = rad_i;
    }

    m_params.m_distance = rad + GetThickness(); // ������� ������ BUG 62631
  }

  VariableParametersOwner::UpdateMeasurePars( gType, recalc );
}


// *** ���-����� ***************************************************************
//------------------------------------------------------------------------------
// ������� ���-�����
// ---
void ShMtBendLine::GetHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  ShMtBaseBendLine::GetHotPoints( hotPoints, editComponent, topComponent, type, pD3DrawTool );

  hotPoints.Add( new HotPointBendLineAngleParam( ehp_ShMtBendAngle, editComponent, topComponent ) ); // ���� �����
}


//-----------------------------------------------------------------------------
// 
//---
std::unique_ptr<HotPointBendLineOperUtil> ShMtBendLine::GetHotPointUtil( MbBendOverSegValues *& params,
  IfComponent & editComponent,
  ShMtBendObject * children )
{
  std::unique_ptr<HotPointBendLineOperUtil> util;

  IfSomething * obj = GetBendObject().obj;
  IFPTR( AnyCurve ) partEdge( obj );
  if ( partEdge ) {
    MbCurve3D * curveEdge = partEdge->GetMathCurve();
    if ( curveEdge )
    {
      PArray<MbFace> arrayFaces( 0, 1, false );
      CollectFaces( arrayFaces, editComponent, children ? children->m_extraName : -1 ); // ������� ��� �����
      if ( arrayFaces.Count() ) {
        params = static_cast<MbBendOverSegValues*>(&PrepareLocalParameters( &editComponent ));

        if ( children )
        {
          CreatorForBendUnfoldParams _creatorBends( *this, &editComponent );
          _creatorBends.Build( 1, children->m_extraName );
          _creatorBends.GetParams( *params, m_params.m_dirAngle );
        }

        if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
          // �������� ��������� ����� ���� ����������
          static_cast<ShMtBendLineParameters&>(GetParameters()).CalcAfterValues( *params );

        // KA V17 �������� ����� ����� �����, � ��� ����� �� ������� ����� �� ���������. ������� ����� 0-��� ����� ��� ���������� �� - ������
        for ( const auto & face : arrayFaces )
        {
          util.reset( new HotPointBendLineOperUtil( face, curveEdge, *params, IsStraighten(), GetThickness() ) );
          if ( util->Init( *params ) )
            break;
        }
      }
    }
  }

  return util;
}


//------------------------------------------------------------------------------
/**
����������� �������������� ���-�����
\param hotPoints -
\param type -
\param pD3DrawTool -
*/
//---
void ShMtBendLine::UpdateHotPoints( RPArray<HotPointParam> & hotPoints,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  if ( hotPoints.Count() )
  {
    MbBendOverSegValues *      params = nullptr;
    std::unique_ptr<HotPointBendLineOperUtil> util( GetHotPointUtil( params,
      hotPoints[0]->GetEditComponent( false/*addref*/ ),
      m_bendObjects.Count() ? m_bendObjects[0] : nullptr ) );
    // �� K11 6.2.2009 ������ �38858:
    // �� K11                                                    nullptr );
    if ( util )
    {
      ShMtBaseBendLine::UpdateHotPoints( *util, hotPoints, *params, pD3DrawTool );
      delete params;
    }
    else
    {
      for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
        hotPoints[i]->SetBasePoint( nullptr );
    }
  }
}


//-----------------------------------------------------------------------------
// ����������� �������������� ���-�����
/**
\param hotPoints - ���-������
\param children - ����, ��� �������� ���������� ���-�����
*/
//---
void ShMtBendLine::UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  ShMtBendObject &         children )
{
  bool del = true;

  // ���-����� ������� ���������� ������ ���� ���� �������� �� ���������� ������ �����
  if ( !children.m_byOwner )
  {
    for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
    {
      HotPointParam * hotPoint = hotPoints[i];

      if ( hotPoint->GetIdent() == ehp_ShMtBendObjectRadius ) // ������ �����
      {
        MbBendOverSegValues *      params = nullptr;
        std::unique_ptr<HotPointBendLineOperUtil> util( GetHotPointUtil( params,
          hotPoints[0]->GetEditComponent( false/*addref*/ ),
          &children ) );
        if ( util )
        {
          del = false;

          HotPointBendLineRadiusParam * hotPointBend = (HotPointBendLineRadiusParam*)hotPoint;
          hotPointBend->params = *params;
          hotPointBend->m_deepness = params->displacement;

          util->UpdateRadiusHotPoint( *hotPoint, *params, (ShMtBendLineParameters&)GetParameters() );

          delete params;
        }
      }
    }
  }

  if ( del )
    for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
    {
      hotPoints[i]->SetBasePoint( nullptr );
    }
}
// *** ���-����� ***************************************************************


//------------------------------------------------------------------------------
/**
����� �� � ������� ���� ��������� ����������� �������
\param  ���
\return ���
*/
//---

bool ShMtBendLine::IsDimensionsAllowed() const
{
  return true;
}


//------------------------------------------------------------------------------
/**
�������� ��������� �������� ������� �� ���-������
\param  dimensions - �������
\param  hotPoints  - ���-�����
\return ���
*/
//---
void ShMtBendLine::UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  const PArray<OperationSpecificDispositionVariant> * /*pVariants*/ )
{
  if ( hotPoints.Count() )
  {
    MbBendOverSegValues *      params = nullptr;
    std::unique_ptr<HotPointBendLineOperUtil> util( GetHotPointUtil( params,
      hotPoints[0]->GetEditComponent( false/*addref*/ ),
      m_bendObjects.Count() ? m_bendObjects[0] : nullptr ) );
    if ( util )
    {
      ShMtBaseBendLine::UpdateDimensionsByHotPoints( dimensions, hotPoints, *util );


      delete params;
    }
  }
}


//------------------------------------------------------------------------------
/**
������������� ���������� ������� � ����������� ��������
\param  dimensions - �������
\return ���
*/
//---
void ShMtBendLine::AssignDimensionsVariables( const SFDPArray<PartObject> & dimensions )
{
  ShMtBaseBendLine::AssignDimensionsVariables( dimensions );
  UpdateMeasurePars( gtt_none, true );
  ::UpdateDimensionVar( dimensions, ehp_ShMtBendAngle, m_params.GetAngleVar() );
}


//------------------------------------------------------------------------------
/**
������� ������� ������� �� ���-������ ��� �������� ��������
\param un -
\param dimensions -
\param hotPoints -
\return ���
*/
//---
void ShMtBendLine::CreateChildrenDimensionsByHotPoints( IfUniqueName * un,
  SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  ShMtBendObject & children )
{
  for ( size_t i = 0, count = hotPoints.Count(); i < count; ++i )
  {
    _ASSERT( hotPoints[i] );
    if ( hotPoints[i]->GetIdent() == ehp_ShMtBendObjectRadius && !children.m_byOwner )
    {
      ::AttachGnrRadialDimension( dimensions, *hotPoints[i], hotPoints[i]->GetIdent() );
    }
  }
  ShMtBaseBendLine::CreateChildrenDimensionsByHotPoints( un, dimensions, hotPoints, children );
}


//------------------------------------------------------------------------------
/**
�������� ��������� �������� ������� �� ���-������ ��� �������� ��������
\param dimensions -
\param hotPoints -
\param children -
\return ���
*/
//---
void ShMtBendLine::UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  ShMtBendObject & children )
{
  // ���-����� ������� ���������� ������ ���� ���� �������� �� ���������� ������ �����
  if ( !children.m_byOwner && hotPoints.Count() )
  {
    IFPTR( OperationCircularDimension ) dimRad( ::FindGnrDimension( dimensions, ehp_ShMtBendObjectRadius, false ) );
    if ( dimRad )
    {
      MbBendOverSegValues *      params = nullptr;
      std::unique_ptr<HotPointBendLineOperUtil> util( GetHotPointUtil( params,
        hotPoints[0]->GetEditComponent( false/*addref*/ ),
        &children ) );

      if ( const MbName * childrenName = children.GetInnerFaceName( 0 ) )
      {
        ShMtBendObject * obj = FindParametrsBendByName( *childrenName );
        if ( obj && util )
          util->UpdateBendRadDimension( *dimRad, obj->GetRadius() );
      }
    }
  }
}


//------------------------------------------------------------------------------
/**
������������� ������� �������� �������� - ������
\param dimensions -
\param children -
\return ���
*/
//---
void ShMtBendLine::AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
  ShMtBendObject        & children )
{
  ::UpdateDimensionVar( dimensions, ehp_ShMtBendObjectRadius, children.m_radius );
}


//-----------------------------------------------------------------------------
/// �������� ���������� ��� ���������
//---
void ShMtBendLine::RenameUnique( IfUniqueName & un ) {
  ShMtBaseBendLine::RenameUnique( un );
  m_params.RenameUnique( un );
}

//------------------------------------------------------------------------------
// 
// ---
bool ShMtBendLine::IsLeftSideFix() {
  return m_params.m_leftFixed;
}


IMP_PERSISTENT_CLASS( kompasDeprecatedAppID, ShMtBendLine );
IMP_PROC_REGISTRATION( ShMtBendLine );


//--------------------------------------------------------------------------------
/**
���������� �� ���������� ��� �������
\param version
\return bool
*/
//---
bool ShMtBendLine::IsNeedSaveAsUnhistored( long version ) const
{
  return __super::IsSavedUnhistoried( version, m_params.m_typeRemoval ) || __super::IsNeedSaveAsUnhistored( version );
}


//------------------------------------------------------------------------------
// ������ �� ������
// ---
void ShMtBendLine::Read( reader &in, ShMtBendLine * obj ) {
  ReadBase( in, static_cast<ShMtBaseBendLine *>(obj) );
  in >> obj->m_params;  // ��������� �������� ������������
}


//------------------------------------------------------------------------------
// ������ � �����
// ---
void ShMtBendLine::Write( writer &out, const ShMtBendLine * obj ) {
  if ( out.AppVersion() < 0x06000033L )
    out.setState( io::cantWriteObject ); // ���������� ��������� ��� ��������� ��������
  else {
    WriteBase( out, static_cast<const ShMtBaseBendLine *>(obj) );
    out << obj->m_params;  // ��������� �������� ������������
  }
}


////////////////////////////////////////////////////////////////////////////////
//
// ������ �������� ��������
//
////////////////////////////////////////////////////////////////////////////////
class ShMtBendHook : public ShMtBaseBendLine,
  public IfShMtBendHook {
public:
  ShMtBendHookParameters  m_params;
  ReferenceContainerFix   m_upToObject;      // ������� �� ������� ������

public:
  I_DECLARE_SOMETHING_FUNCS;

  // ����������� ��� ������������� � ������������ ��������
  ShMtBendHook( const ShMtBendHookParameters &, IfUniqueName * un );
  // ����������� ������������
  ShMtBendHook( const ShMtBendHook & );

public:
  virtual bool                     HasBendIgnoreOwner() const; // ���� true, �� ���� �� ���� �������� ����� ����� ��������� �������� �� ����, ���� ���� �� ������ �� ���������� ����
#ifdef MANY_THICKNESS_BODY_SHMT
  virtual bool                     IsSuitedObject( const WrapSomething & );
#else
  virtual bool                     IsSuitedObject( const WrapSomething &, double );
#endif
  virtual double                   GetOwnerAbsAngle() const;
  virtual double                   GetOwnerValueBend() const;
  virtual double                   GetOwnerAbsRadius() const;    // ���������� ������ �����
  virtual bool                     IsOwnerInternalRad() const;    // �� ����������� �������?
  virtual UnfoldType               GetOwnerTypeUnfold() const;    // ���� ��� ���������  
  virtual void                     SetOwnerTypeUnfold( UnfoldType t );  // ������ ��� ���������  
  virtual bool                     GetOwnerFlagByBase() const;  // 
  virtual void                     SetOwnerFlagByBase( bool val );        // 
  virtual double                   GetOwnerCoefficient() const;  // �����������
  virtual bool                     IfOwnerBaseChanged( IfComponent * trans );        // ���� ���������� ����� ���� ���������, � ���� ������ ���� �� ����, �� ��������� ���� 
  virtual double                   GetThickness() const;  // ���� �������
  virtual void                     SetThickness( double );// ������ �������      
                                                          /// ����� ��� ��� ����������� ��������������
  virtual void                     EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp );

  virtual void                     SetParameters( const ShMtBaseBendParameters & );
  virtual ShMtBaseBendParameters & GetParameters() const;
  virtual bool                     IsLeftSideFix();
  //--------------- ����� ������� ������� ������ -------------------------------//
  virtual uint                     GetObjType() const;                            // ������� ��� �������
  virtual uint                     GetObjClass() const;                            // ������� ����� ��� �������
  virtual PrObject               & Duplicate() const;

  virtual bool                     RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath ); // ������������ �����
  virtual void                     ClearReferencedObjects( bool soft );
  virtual bool                     ChangeReferences( ChangeReferencesData & changedRefs ); ///< �������� ������ �� �������
  virtual bool                     IsValid() const;  // �������� �������������� �������
  virtual void                     WhatsWrong( WarningsArray & ); // �������� ������ �������������� �� �������
  virtual bool                     IsThisParent( uint8 relType, const WrapSomething & ) const;
  virtual void                     PrepareToBuild( const IfComponent & ); ///< ������������� � ����������
  virtual void                     PostBuild(); ///< ����� �������� ����� ����������
  virtual bool                     CopyInnerProps( const IfPartObject& source,
    uint sourceSubObjectIndex,
    uint destSubObjectIndex ) override;
  /// ���� �� ��������� ��� ������ ��� �������
  virtual bool                     IsNeedSaveAsUnhistored( long version ) const override;

  /// ����������� ����, ����������� ����
  virtual uint            PrepareLocalSolid( MbCurve3D            & curve,
    RPArray<MbFace>      & mbFaces,
    MbSNameMaker         & name,
    MbSolid              & prevSolid,
    MbBendValues         & params,        // ���� ��� �������������� ���������
    const IfComponent    * /*operOwner*/, // ��������� - �������� ���� ��������
    const IfComponent    * /*bodyOwner*/, // ��������� - �������� ����, ��� ������� ��� �������� �������������
    WhatIsDone             wDone,
    uint                   idShellThread,
    MbSolid              *&currSolid );
  // ����������� ���� ���������
  virtual MbBendValues &  PrepareLocalParameters( const IfComponent * component );

  // �������� ��� ������� UptoObject
  virtual bool                  SetResetUpToObject( const WrapSomething & );
  virtual const WrapSomething & GetUpToObject() const;
  // ������� UptoObject
  virtual void                  ResetUpToObject();

  /// �������� �� ��� ��������� �������� ������
  virtual bool                  IsCanSetTolerance( const VarParameter & varParameter ) const;
  /// ��� ��������� �������� ����������� ���������� ������
  virtual double                GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const;
  /// �������� �� �������� �������
  virtual bool                  ParameterIsAngular( const VarParameter & varParameter ) const;

  // ���������� IfHotPoints
  /// ������� ���-�����               
  virtual void GetHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool );
  /// ����������� �������������� ���-�����
  virtual void UpdateHotPoints( RPArray<HotPointParam> & hotPoints,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool );
  /// ���������� ��� �������������� ���-����� 
  // step - ��� ������������, ��� ����������� �������� � �������� ������������� �����
  virtual bool ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
    double step, D3Draw & pD3DrawTool );
  /// ����������� �������������� ���-�����
  virtual void UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    ShMtBendObject &         children ) override;

  // ���������� ���-����� �������� ������ �� ShMtStraighten
  /// ������� ���-����� (��������������� ������� � 5000 �� 5999)
  virtual void GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ShMtBendObject &         children );
  /// ���������� ��� �������������� ���-�����
  virtual bool ChangeChildrenHotPoint( HotPointParam &  hotPoint,
    const MbVector3D &     vector,
    double           step,
    ShMtBendObject & children );
  /// �������� ���������� ��� ���������
  virtual void RenameUnique( IfUniqueName & un );

  /// ����� �� � ������� ���� ��������� ����������� �������
  virtual bool IsDimensionsAllowed() const;
  /// ������������� ������� � ����������� ��������
  virtual void AssignDimensionsVariables( const SFDPArray<PartObject> & dimensions );
  /// ������� ������� ������� �� ���-������
  virtual void CreateDimensionsByHotPoints( IfUniqueName * un,
    SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints );
  /// �������� ��������� �������� ������� �� ���-������
  virtual void UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    const PArray<OperationSpecificDispositionVariant>* pVariants );

  /// ������������� ������� �������� �������� - ������
  virtual void AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
    ShMtBendObject & children );
  /// ������� ������� ������� �� ���-������ ��� �������� ��������
  virtual void CreateChildrenDimensionsByHotPoints( IfUniqueName * un,
    SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children );
  /// �������� ��������� �������� ������� �� ���-������ ��� �������� ��������
  virtual void UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children );
  /// ������ ���� ��� ��������� ������� ���������� �� �������
  virtual void UpdateMeasurePars( GeneralToleranceType gType, bool recalc );

private:
  std::unique_ptr<HotPointBendHookOperUtil> GetHotPointUtil( MbJogValues   *& params,
    MbBendValues   & secondBendParams,
    IfComponent    & editComponent,
    ShMtBendObject * children );

  /// �������� ���-�����
  void HideAllHotPoints( RPArray<HotPointParam> & hotPoints );

  /// �������� ���-����� ������
  void UpdateDistanceExtrHotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBendHookOperUtil & util,
    MbBendValues secondBendParameters );
  /// �������� ���-����� �������, ��������� � �� ����� ��������� �� ��������� �������
  void UpdateRadiusHotPointByBendObjects( RPArray<HotPointParam> & hotPoints,
    HotPointBendHookOperUtil & util,
    MbBendOverSegValues & bendParameters );
  /// ������� �� ����������� ������� ���-����� ������� �� ������ �����
  bool IsRadiusHotPointByFirstBend();
  /// ������� �� ����������� ������� ���-����� ������� �� ������ �����
  bool IsRadiusHotPointBySecondBend();
  /// ���������� ���������� ��� �������������� ���-����� �������, � ����������� �� 
  /// ���������� ������ �� ��������� ������� ��� ���
  bool ChangeRadiusHotPointByBendObjects( HotPointParam & radiusHotPoint, const MbVector3D & vector,
    double step, D3Draw & pD3DrawTool );

  /// �������� ������ ����������
  void UpdateLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints );
  /// �������� ������ �������
  void UpdateRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    int radiusHotPointId,
    EDimensionsIds radiusDimensionId,
    bool moveToInternal );
  /// �������� ������ ����
  void UpdateAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints );

  /// ������� ��������� ������� ����
  bool GetAngleDimensionParameters( const RPArray<HotPointParam> & hotPoints,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// �������� ��������� ������� ���������� - �������
  bool GetLengthDimensionParametersByOut( const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// �������� ��������� ������� ���������� - ������
  bool GetLengthDimensionParametersByIn( const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// �������� ��������� ������� ���������� - ������
  bool GetLengthDimensionParametersByAll( const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// �������� ������ ������
  void ResetLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions );
  /// �������� ������ ������
  void ResetAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions );
  /// �������� ������ �������
  void ResetRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions, EDimensionsIds radiusDimensionId );

  bool GetRadiusDimensionParameters( HotPointBendLineRadiusParam * radiusHotPoint, bool moveToInternal,
    MbPlacement3D & dimensionPlacement, MbArc3D & dimensionArc );

private:
  void operator = ( const ShMtBendHook & ); // �� �����������

  DECLARE_PERSISTENT_CLASS( ShMtBendHook )
};



//------------------------------------------------------------------------------
// �����������, ������ ���������� ��������� ������ �� �����������, ����� ����� �����
// ����������� ��� ������������� � ������������ ��������
ShMtBendHook::ShMtBendHook( const ShMtBendHookParameters & _pars, IfUniqueName * un )
  : ShMtBaseBendLine( un ),
  m_upToObject( 1 ),
  m_params( _pars )
{
  if ( un )
    m_params.RenameUnique( *un );
}


//------------------------------------------------------------------------------
// ����������� ������������
// ---
ShMtBendHook::ShMtBendHook( const ShMtBendHook & other )
  : ShMtBaseBendLine( other ),
  m_upToObject( other.m_upToObject ),
  m_params( other.m_params )
{
}


//------------------------------------------------------------------------------
//
// ---
ShMtBendHook::ShMtBendHook( TapeInit )
  : ShMtBaseBendLine( tapeInit ),
  m_upToObject( 1 ),
  m_params( nullptr ) // ���������
{
}


IfSomething * ShMtBendHook::QueryInterface( unsigned int iid ) const {
  IfSomething * res = ShMtBaseBendLine::QueryInterface( iid );
  if ( !res ) {
    if ( iid == iidr_ShMtBendHook ) {
      AddRef();
      return (IfShMtBendHook*)this;
    }
  }

  return res;
}


I_IMP_SOMETHING_FUNCS_AR( ShMtBendHook )


uint ShMtBendHook::GetObjType()  const { return ot_ShMtBendHook; } // ������� ��� �������
uint ShMtBendHook::GetObjClass() const { return oc_ShMtBendHook; } // ������� ����� ��� �������

                                                                   //-----------------------------------------------------------------------------
                                                                   //
                                                                   //---
PrObject & ShMtBendHook::Duplicate() const
{
  return *new ShMtBendHook( *this );
}


//------------------------------------------------------------------------------
// 
// ---
bool ShMtBendHook::IsLeftSideFix() {
  return m_params.m_leftFixed;
}


//------------------------------------------------------------------------------
// ������ ���������
// ---
void ShMtBendHook::SetParameters( const ShMtBaseBendParameters & p ) {
  m_params = (ShMtBendHookParameters&)p;
}


//------------------------------------------------------------------------------
// �������� ���������
// ---
ShMtBaseBendParameters & ShMtBendHook::GetParameters() const {
  return const_cast<ShMtBendHookParameters&>(m_params);
}


//------------------------------------------------------------------------------
// �������� �������������� ��������
// ---
bool ShMtBendHook::IsValid() const {
  bool res = ShMtBaseBendLine::IsValid() && m_params.IsValidParam();
  if ( res )
    if ( m_params.m_typeExtrusion != ShMtBendHookParameters::be_blind )
      res = !m_upToObject.IsEmpty();

  return res;
}


//------------------------------------------------------------------------------
// ������������ �����
// ---
bool ShMtBendHook::RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath )
{
  bool res = ShMtBaseBendLine::RestoreReferences( result, allRefs, withMath );
  bool upToRes = m_upToObject.RestoreReferences( result, allRefs, withMath );
  if ( m_params.m_typeExtrusion != ShMtBendHookParameters::be_blind )
    res = res && upToRes;

  return res;
} // RestoreReferences


  //------------------------------------------------------------------------------
  // �������� ������� ������� soft = true - ������ ������� (�� ������ �����)
  // ---
void ShMtBendHook::ClearReferencedObjects( bool soft )
{
  ShMtBaseBendLine::ClearReferencedObjects( soft );
  m_upToObject.ClearObjects( soft );
}


//------------------------------------------------------------------------------
// �������� ������ �� �������
// ---
bool ShMtBendHook::ChangeReferences( ChangeReferencesData & changedRefs )
{
  bool res = ShMtBaseBendLine::ChangeReferences( changedRefs );
  if ( m_upToObject.ChangeReferences( changedRefs ) )
    res = true;

  return res;
}


//--------------------------------------------------------------------------------
// �������� ��� ������� UptoObject
//---
bool ShMtBendHook::SetResetUpToObject( const WrapSomething & wrapper ) {
  bool noFind = m_upToObject.FindObject( wrapper ) == -1;
  if ( noFind ) {
    m_upToObject.SetObject( 0, wrapper );
  }
  else
    m_upToObject.SetObject( 0, nullptr );

  return noFind;
}


//--------------------------------------------------------------------------------
// UptoObject
//---
const WrapSomething & ShMtBendHook::GetUpToObject() const {
  return m_upToObject.GetObjectTrans( 0 );
}


//--------------------------------------------------------------------------------
// ������� UptoObject
//---
void ShMtBendHook::ResetUpToObject() {
  m_upToObject.SetObject( 0, nullptr );
}


//------------------------------------------------------------------------------
// �������� ������ �������������� �� �������
// ---
void ShMtBendHook::WhatsWrong( WarningsArray & warnings ) {
  ShMtBaseBendLine::WhatsWrong( warnings );

  if ( m_params.m_typeExtrusion != ShMtBendHookParameters::be_blind && m_upToObject.IsEmpty() )
    m_upToObject.WhatsWrong( warnings );

  m_params.WhatsWrongParam( warnings );

  bool yes = true;
  for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ ) {
    if ( !m_params.m_typeAngle && ::fabs( m_bendObjects[i]->GetAbsAngle() ) + PARAM_NEAR >= M_PI ) {
      // ����������� ���� ������ ���� ������ 360 ��������.
      warnings.AddWarning( IDP_SHMT_ERROR_BEND_HOOK_ADDED_ANGLE, module );
      yes = false;
      break;
    }
  }

  if ( yes )
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ ) {
      // ���� ������ ���� ������ 180 ��.
      if ( ::fabs( m_bendObjects[i]->GetAbsAngle() ) + PARAM_NEAR >= M_PI ) {
        warnings.AddWarning( IDP_SHMT_ERROR_BEND_HOOK_ANGLE, module );
        break;
      }
    }
}


//------------------------------------------------------------------------------
//
// ---
bool ShMtBendHook::IsThisParent( uint8 relType, const WrapSomething & parent ) const {
  return ShMtBaseBendLine::IsThisParent( relType, parent ) || m_upToObject.IsThisParent( rlt_Strong, parent );
}


//------------------------------------------------------------------------------
// ����� ��� ��� ����������� ��������������
// ---
void ShMtBendHook::EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp ) {
  IFPTR( ShMtBendLine ) ini( const_cast<IfSheetMetalOperation*>(&iniObj) );
  if ( ini ) {
    m_params.EditingEnd( ini->GetParameters(), *editComp.ReturnPartRebuildResult(), this );
  }
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendHook::GetThickness() const {
  return m_params.m_thickness;    // �������
}


//---------------------------------------------------------------------------------
//
//---
void ShMtBendHook::SetThickness( double h ) {
  m_params.m_thickness = h;    // �������
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendHook::GetOwnerAbsRadius() const {
  //	return m_params.GetRadius();    // ������ �����
  return m_params.m_internal ? m_params.GetRadius() : (m_params.GetRadius() - GetThickness());    // ���������� ������ �����
}


//---------------------------------------------------------------------------------
// �����������, ������������ ��������� ������������ ����
//---
double ShMtBendHook::GetOwnerValueBend() const {
  double val;
  m_params.GetValueBend( val );
  return val;
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendHook::GetOwnerAbsAngle() const {
  return  m_params.GetAbsAngle();
}


//---------------------------------------------------------------------------------
// �� ����������� �������  
//---
bool ShMtBendHook::IsOwnerInternalRad() const {
  return m_params.m_internal;
}


//---------------------------------------------------------------------------------
// ���� ��� ���������  
//---
UnfoldType ShMtBendHook::GetOwnerTypeUnfold() const {
  return m_params.m_typeUnfold;
}


//---------------------------------------------------------------------------------
// ������ ��� ���������  
//---
void ShMtBendHook::SetOwnerTypeUnfold( UnfoldType t ) {
  m_params.m_typeUnfold = t;
}


//------------------------------------------------------------------------
// ���� true, �� ���� �� ���� �������� ����� ����� ��������� �������� �� ����, ���� ���� �� ������ �� ���������� ����
//---
bool ShMtBendHook::HasBendIgnoreOwner() const {
  bool newAngle = false;
  if ( m_bendObjects.Count() == 2 ) {
    // ���� ������� �������� ������ �����������, �� ���� ��������� ����
    double hMin = m_bendObjects[0]->GetAbsRadius() + m_bendObjects[1]->GetAbsRadius() + GetThickness();
    if ( (m_params.GetHeight() - Math::LengthEps) < (hMin * (1 - ::cos( m_bendObjects[0]->GetAbsAngle() ))) )
      newAngle = true;
  }

  return newAngle;
}


//---------------------------------------------------------------------------------
//
//---
bool ShMtBendHook::GetOwnerFlagByBase() const {
  return m_params.GetFlagUnfoldByBase();
}


//---------------------------------------------------------------------------------
//
//---
void ShMtBendHook::SetOwnerFlagByBase( bool val ) {
  m_params.SetFlagUnfoldByBase( val );
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendHook::GetOwnerCoefficient() const {
  return m_params.GetCoefficient();
}


//---------------------------------------------------------------------------------
// ���� ���������� ����� ���� ���������, � ���� ������ ���� �� ����, �� ��������� ���� 
//---
bool ShMtBendHook::IfOwnerBaseChanged( IfComponent * trans ) {
  return m_params.ReadBaseIfChanged( trans );
}


//------------------------------------------------------------------------------
// ������� �� ��������� ������
// ---
#ifdef MANY_THICKNESS_BODY_SHMT
bool ShMtBendHook::IsSuitedObject( const WrapSomething & wrapper )
#else
bool ShMtBendHook::IsSuitedObject( const WrapSomething & wrapper, double thickness )
#endif
{
  bool res = false;
  // �� K11 17.12.2008 ������ �37142: ��������� ����� ����� ������������ ���
  if ( wrapper && wrapper.component == wrapper.editComponent && !::IsMultiShellBody( wrapper ) )
  {
#ifdef MANY_THICKNESS_BODY_SHMT
    res = ShMtBaseBendLine::IsSuitedObject( wrapper );
#else
    res = ShMtBaseBendLine::IsSuitedObject( wrapper, thickness );
#endif

    if ( !res )
      if ( m_params.m_typeExtrusion != ShMtBendHookParameters::be_blind )
      {
        if ( m_params.m_typeExtrusion == ShMtBendHookParameters::be_upToVertex )
          res = !!IFPTR( AnyPoint )(wrapper.obj);
        else
          res = !!IFPTR( AnySurface )(wrapper.obj);
      }
  }

  return res;
}


//------------------------------------------------------------------------------
// ������������� � ����������
//---
void ShMtBendHook::PrepareToBuild( const IfComponent & operOwner ) // ��������� - �������� ���� ��������
{
  creatorBendHooks = new CreatorForBendHookUnfoldParams( *this, (IfComponent*)&operOwner );
}


//------------------------------------------------------------------------------
// ����� �������� ����� ����������
//---
void ShMtBendHook::PostBuild()
{
  delete creatorBendHooks;
  creatorBendHooks = nullptr;
}


//------------------------------------------------------------------------------
//
//---
bool ShMtBendHook::CopyInnerProps( const IfPartObject& source,
  uint sourceSubObjectIndex,
  uint destSubObjectIndex )
{
  bool result = ShMtBaseBendLine::CopyStyleInnerProps( source );

  if ( const ShMtBendHook* pSource = dynamic_cast<const ShMtBendHook*>(&source) ) {
    m_params = pSource->m_params;
  }

  return result;
}


//------------------------------------------------------------------------------
// ����������� ���� ���������
// ---
MbBendValues & ShMtBendHook::PrepareLocalParameters( const IfComponent * component ) {
  MbJogValues * params = new MbJogValues();
  IFPTR( AnyCurve ) anyCurve( GetObject().obj );
  if ( anyCurve ) {
    MbCurve3D * curve = anyCurve->GetMathCurve();
    if ( curve && !m_bendFaces.IsEmpty() ) {
      const RefByName & ref = *m_upToObject[0];
      IFPTR( AnySurface ) anySurf( m_bendFaces.GetObjectTrans( 0 ).obj );
      if ( anySurf ) {
        m_params.GetDepth( ref, *anySurf, curve, component );
        if ( component ) {
          m_params.ReadBaseIfChanged( const_cast<IfComponent*>(component) ); // ���� ��������� ��������� ����� �� ����, �� ���������� ����, ���� ����
          m_params.GetValues( *params );

          // CC K8 � ������ ������� ���������� �������� ������������ �����
          if ( GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0800000BL )
            m_params.CalcAfterValues( *params );

        }
      }
    }
  }
  return *params;
}


//---------------------------------------------------------------------------------
// ����������� ����, ����������� ����
//---
uint ShMtBendHook::PrepareLocalSolid( MbCurve3D            & curve,
  RPArray<MbFace>      & mbFaces,
  MbSNameMaker         & nameMaker,
  MbSolid              & prevSolid,
  MbBendValues         & params,    // ���� ��� �������������� ���������
  const IfComponent    * operOwner, // ��������� - �������� ���� ��������
  const IfComponent    * bodyOwner, // ��������� - �������� ����, ��� ������� ��� �������� �������������
  WhatIsDone             wDone,
  uint                   idShellThread,
  MbSolid              *&result )
{
  MbResultType codeError = operOwner ? rt_Success : rt_ParameterError;

  if ( operOwner )
  {
    MbBendValues secondBendParams;
    if ( creatorBendHooks )
    {
      creatorBendHooks->Build( 2, ((MbJogValues&)params).elevation, GetObjVersion(), idShellThread );
      creatorBendHooks->m_prepare = wDone != wd_Phantom;

      RPArray<MbBendValues> parsBend;
      parsBend.Add( &params );
      parsBend.Add( &secondBendParams );
      creatorBendHooks->GetParams( parsBend, m_params.m_dirAngle );
    }

    // CC K8 ��� ����� ������ ���������� �������� ���� ����������� � ����� �����
    if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
      m_params.CalcAfterValues( (MbBendOverSegValues&)params );

    bool yes = true;
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
    {
      // ���� ������ ���� ������ 180 ��.
      if ( ::fabs( m_bendObjects[i]->GetAbsAngle() ) + PARAM_NEAR >= M_PI )
      {
        yes = false;
        codeError = rt_Error;
        break;
      }
    }


    if ( yes && IsValidParamBaseTable() )
    {
      PArray<MbFace> par_1( 0, 1, false ); // ����� ������� ����� ��������
      PArray<MbFace> par_2( 0, 1, false ); // ����� ������� ����� ��������

      codeError = ::SheetSolidJog( prevSolid,                                   // �������� ����
        wDone == wd_Phantom ? cm_Copy
        : cm_KeepHistory,        // �� �������� � ������ �������� ��������� ����
        mbFaces,                                     // ���������� �����
        curve,                                       // ������������� ������, ����� ������� �����
                                                     /*����������� ������ 40270*/
        GetObjVersion().GetVersionContainer().GetAppVersion() > 0x0C000001L || GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0A000001L ? false : IsStraighten(),                  // �����
        (MbJogValues&)params,                        // ��������� ��������� ����
        secondBendParams,                            // ��������� ������� �����
        nameMaker,                                        // �����������
        par_1,                                       // ����� ������� ����� ��������
        par_2,                                       // ����� ������� ����� ��������
        result );                                    // �������������� ����

      if ( creatorBendHooks && wDone != wd_Phantom )
      {
        creatorBendHooks->SetResult( result );
        if ( result && codeError == rt_Success )
          // ���� ��������� ��� ����� � �� �������
          creatorBendHooks->InitFaces( par_1, par_2, MainId(), GetObjVersion() );
      }
    }
  }

  return codeError;
}


//------------------------------------------------------------------------------
// �������� �� ��� ��������� �������� ������
// ---
bool ShMtBendHook::IsCanSetTolerance( const VarParameter & varParameter ) const
{
  if ( IsSubBendsParameter( varParameter ) )
  {
    return IsCanSetToleranceOnSubBendsParameter( varParameter );
  }
  else
  {
    if ( m_params.IsContainsVarParameter( varParameter ) )
      return m_params.IsCanSetTolerance( varParameter );
    else
      return ShMtBaseBendLine::IsCanSetTolerance( varParameter );
  }
}


//------------------------------------------------------------------------------
// �������� �� �������� �������
// ---
bool ShMtBendHook::ParameterIsAngular( const VarParameter & varParameter ) const
{
  if ( &varParameter == &const_cast<ShMtBendHookParameters&>(m_params).GetAngleVar() )
    return  true;

  return false;
}


//------------------------------------------------------------------------------
// ��� ��������� �������� ����������� ���������� ������
// ---
double ShMtBendHook::GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const
{
  if ( &varParameter == &const_cast<ShMtBendHookParameters&>(m_params).GetAngleVar() )
  {
    //    double angle = m_params.m_typeAngle ? m_params.GetAngle() : 180 - m_params.GetAngle();
    // CC K14 ��� ��������� ������� ���������� ������. ����������� � ����������
    return reader.GetTolerance( m_params.m_distance, // ������� ������ BUG 62631
      vd_angle,
      tType );
  }
  return ShMtBaseBend::GetGeneralTolerance( varParameter, tType, reader );
}


//------------------------------------------------------------------------------------
/// ������ ���� ��� ��������� ������� ���������� �� �������
/**
// \method    InitGeneralTolerances
// \access    public
// \return    void
// \qualifier
*/
//---
void ShMtBendHook::UpdateMeasurePars( GeneralToleranceType gType, bool recalc )
{
  if ( recalc )
  {
    // � ������ ����� ���������� ������
    double rad = MB_MAXDOUBLE;
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
    {
      double rad_i = m_bendObjects[i]->GetAbsRadius();
      if ( rad_i < rad )
        rad = rad_i;
    }

    m_params.m_distance = rad + GetThickness(); // ������� ������ BUG 62631
  }

  VariableParametersOwner::UpdateMeasurePars( gType, recalc );
}


// *** ���-����� ***************************************************************
//------------------------------------------------------------------------------
// ������� ���-�����
// ---
void ShMtBendHook::GetHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  ShMtBaseBendLine::GetHotPoints( hotPoints, editComponent, topComponent, type, pD3DrawTool );

  hotPoints.Add( new HotPointHookHeightParam( ehp_ShMtBendDistanceExtr, editComponent, topComponent ) ); // ������ 
  hotPoints.Add( new HotPointBendAngleParam( ehp_ShMtBendAngle, editComponent, topComponent ) ); // ���� �����
}


//-----------------------------------------------------------------------------
// ���������� ��� �������������� ���-����� 
/**
\param hotPoint - ��������� ���-�����
\param vector - ������ ������
\param step - ��� ������������, ��� ����������� �������� � �������� ������������� �����
*/
//---
bool ShMtBendHook::ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
  double step, D3Draw & pD3DrawTool )
{
  bool result = false;

  switch ( hotPoint.GetIdent() )
  {
  case ehp_ShMtBendDistanceExtr:
    result = ::SetHotPointVector( (HotPointHookHeightParam &)hotPoint, vector, step, m_params.m_distanceExtr, 0.0, 5E+5 );
    break;
  case ehp_ShMtBendRadius:
    result = ((HotPointBendRadiusParam &)hotPoint).SetRadiusHotPoint( vector, step, GetParameters(), GetThickness() );
    break;
  default:
    result = ShMtBaseBendLine::ChangeHotPoint( hotPoint, vector, step, pD3DrawTool );
    break;
  };

  return result;
}


//------------------------------------------------------------------------------
/**
�������� ���-�����
\param hotPoints - ������ ���-�����
*/
//--- 
void ShMtBendHook::HideAllHotPoints( RPArray<HotPointParam> & hotPoints )
{
  for ( size_t i = 0, count = hotPoints.Count(); i < count; i++ )
  {
    if ( hotPoints[i] != nullptr )
      hotPoints[i]->SetBasePoint( nullptr );
  }
}


//------------------------------------------------------------------------------
/**
�������� ���-����� ������
\param hotPoints - ������ ���-�����
\param util - ������� ��� ������� ���-�����
\param secondBendParameters - ��������� �����
\param drawTool - ���������� ���������
*/
//--- 
void ShMtBendHook::UpdateDistanceExtrHotPoint( RPArray<HotPointParam> & hotPoints,
  HotPointBendHookOperUtil & util,
  MbBendValues secondBendParameters )
{
  HotPointHookHeightParam * distanceExtrHotPoint = dynamic_cast<HotPointHookHeightParam *>(FindHotPointWithId( ehp_ShMtBendDistanceExtr, hotPoints ));

  if ( distanceExtrHotPoint != nullptr )
  {
    distanceExtrHotPoint->params = secondBendParameters;
    util.UpdateHeightHotPoint( *distanceExtrHotPoint, secondBendParameters );
  }
}


//------------------------------------------------------------------------------
/**
������� �� ����������� ������� ���-����� ������� �� ������ �����
\return ������� �� ����������� ������� ���-����� ������� �� ������ �����
*/
//--- 
bool ShMtBendHook::IsRadiusHotPointBySecondBend()
{
  if ( m_bendObjects.Count() < 2 )
    return false;
  if ( m_bendObjects[1] == nullptr )
    return false;

  if ( m_bendObjects[1]->m_byOwner )
    return true;
  else
    return false;
}


//------------------------------------------------------------------------------
/**
������� �� ����������� ������� ���-����� ������� �� ������ �����
\return ������� �� ����������� ������� ���-����� ������� �� ������ �����
*/
//--- 
bool ShMtBendHook::IsRadiusHotPointByFirstBend()
{
  if ( m_bendObjects.Count() == 0 )
    return false;
  if ( m_bendObjects[0] == nullptr )
    return false;

  if ( m_bendObjects[0]->m_byOwner )
    return true;
  else
    return false;
}


//------------------------------------------------------------------------------
/**
�������� ���-����� �������, ��������� � �� ����� ��������� �� ��������� �������
\param hotPoints - ������ ���-�����
\param util - ������� ��� ������� ���-�����
*/
//--- 
void ShMtBendHook::UpdateRadiusHotPointByBendObjects( RPArray<HotPointParam> & hotPoints,
  HotPointBendHookOperUtil & util,
  MbBendOverSegValues & bendParameters )
{
  HotPointBendLineRadiusParam * radiusHotPoint = dynamic_cast<HotPointBendLineRadiusParam *>(FindHotPointWithId( ehp_ShMtBendRadius, hotPoints ));
  if ( radiusHotPoint == nullptr )
    return;

  if ( m_bendObjects.Count() != 0 )
  {
    if ( IsRadiusHotPointByFirstBend() )
    {
      ShMtBaseBendLine::UpdateRadiusHotPoint( hotPoints, util, bendParameters );
    }
    else
    {
      if ( IsRadiusHotPointBySecondBend() )
      {
        radiusHotPoint->params = bendParameters;
        radiusHotPoint->params.radius = m_bendObjects[1]->m_radius;
        radiusHotPoint->m_deepness = bendParameters.displacement;
        util.UpdateRadiusHotPointOnSecondBend( m_bendObjects[1]->m_radius, m_params.m_internal, *radiusHotPoint );
      }
    }
  }
  else
  {
    radiusHotPoint->SetBasePoint( nullptr );
  }
}


//------------------------------------------------------------------------------
/**
����������� �������������� ���-�����
\param hotPoints -
\param type -
\param pD3DrawTool -
*/
//---
void ShMtBendHook::UpdateHotPoints( RPArray<HotPointParam> & hotPoints,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  if ( hotPoints.Count() == 0 )
    return;

  MbJogValues * jogParameters = nullptr;
  MbBendValues secondBendParameters;
  std::unique_ptr<HotPointBendHookOperUtil> util( GetHotPointUtil( jogParameters, secondBendParameters, hotPoints[0]->GetEditComponent( false ),
    m_bendObjects.Count() ? m_bendObjects[0] : nullptr ) );

  if ( util != nullptr && jogParameters != nullptr )
  {
    UpdateDirectionHotPoint( hotPoints, *util );
    UpdateDirection2HotPoint( hotPoints, *util );
    UpdateRadiusHotPointByBendObjects( hotPoints, *util, *jogParameters );
    UpdateAngleHotPoint( hotPoints, *util, *jogParameters );
    UpdateDistanceExtrHotPoint( hotPoints, *util, secondBendParameters );
  }
  else
  {
    HideAllHotPoints( hotPoints );
  }

  if ( jogParameters != nullptr )
    delete jogParameters;
}


//-----------------------------------------------------------------------------
// 
//---
std::unique_ptr<HotPointBendHookOperUtil> ShMtBendHook::GetHotPointUtil( MbJogValues *& params,
  MbBendValues & secondBendParams,
  IfComponent & editComponent,
  ShMtBendObject * children )
{
  std::unique_ptr<HotPointBendHookOperUtil> util;

  IfSomething * obj = GetBendObject().obj;
  IFPTR( AnyCurve ) partEdge( obj );
  if ( partEdge ) {
    MbCurve3D * curveEdge = partEdge->GetMathCurve();
    if ( curveEdge )
    {
      PArray<MbFace> arrayFaces( 0, 1, false );
      CollectFaces( arrayFaces, editComponent, children ? children->m_extraName : -1 ); // ������� ��� �����
      if ( arrayFaces.Count() ) {
        params = (MbJogValues*)&PrepareLocalParameters( &editComponent );

        if ( children )
        {
          CreatorForBendHookUnfoldParams creatorBends( *this, &editComponent );
          creatorBends.Build( 2, params->elevation, GetObjVersion(), children->m_extraName );
          RPArray<MbBendValues> parsBend;
          parsBend.Add( params );
          parsBend.Add( &secondBendParams );
          creatorBends.GetParams( parsBend, m_params.m_dirAngle );
        }
        /*
        CreatorForBendHookUnfoldParams creatorBends( *this, &editComponent, params->elevation, GetObjVersion() );
        creatorBends.GetParams( *params, secondBendParams, m_params.m_dirAngle );
        */
        if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
          // �������� ��������� ����� ���� ����������
          m_params.CalcAfterValues( *params );


        for ( const auto & face : arrayFaces )
        {
          util.reset( new HotPointBendHookOperUtil( face, curveEdge, *params, IsStraighten(), GetThickness() ) );
          double angleNominal = m_params.m_typeAngle ? m_params.GetAngle()* Math::DEGRAD :
            M_PI - m_params.GetAngle()* Math::DEGRAD;
          if ( !m_params.m_dirAngle )
            angleNominal = -angleNominal;

          if ( util->InitNext( *params, secondBendParams, angleNominal, m_params.m_dirAngle ) )
            break;
        }
      }
    }
  }

  return util;
}


//------------------------------------------------------------------------------
// ������� ���-����� (��������������� ������� � 5000 �� 5999)
// ---
void ShMtBendHook::GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ShMtBendObject &         children )
{
  hotPoints.Add( new HotPointBendHookObjectRadiusParam( ehp_ShMtBendObjectRadius,
    editComponent, topComponent ) ); // ������ ����� 
}


//-----------------------------------------------------------------------------
// ���������� ��� �������������� ���-����� 
/**
\param hotPoint - ��������������� ���-�����
\param vector - ������ ��������
\param step - ��� ������������, ��� ����������� �������� � �������� ������������� �����
\param children - ����, ��� �������� ���������� ���-�����
*/
//---
bool ShMtBendHook::ChangeChildrenHotPoint( HotPointParam &  hotPoint,
  const MbVector3D &     vector,
  double           step,
  ShMtBendObject & children )
{
  bool ret = false;

  if ( children.m_ind == 1 )
    ret = ShMtBaseBendLine::ChangeChildrenHotPoint( hotPoint, vector, step, children );
  else
  {
    if ( hotPoint.GetIdent() == ehp_ShMtBendObjectRadius ) // ������ �����
      ret = ((HotPointBendHookObjectRadiusParam &)hotPoint).SetRadiusHotPoint_2( vector, step,
        GetParameters(),
        children );
  }

  return ret;
}


//-----------------------------------------------------------------------------
/// �������� ���������� ��� ���������
//---
void ShMtBendHook::RenameUnique( IfUniqueName & un ) {
  ShMtBaseBendLine::RenameUnique( un );
  m_params.RenameUnique( un );
}


//------------------------------------------------------------------------------
/**
����� �� � ������� ���� ��������� ����������� �������
\return ����� �� � ������� ���� ��������� ����������� �������
*/
//--- 
bool ShMtBendHook::IsDimensionsAllowed() const
{
  return true;
}


//------------------------------------------------------------------------------
/**
������������� ������� � ����������� ��������
\param dimensions - �������
*/
//--- 
void ShMtBendHook::AssignDimensionsVariables( const SFDPArray<PartObject> & dimensions )
{
  ShMtBaseBendLine::AssignDimensionsVariables( dimensions );
  UpdateMeasurePars( gtt_none, true );
  ::UpdateDimensionVar( dimensions, ed_length, m_params.m_distanceExtr );
  ::UpdateDimensionVar( dimensions, ed_radius, m_params.m_radius );
  ::UpdateDimensionVar( dimensions, ed_angle, m_params.GetAngleVar() );
}


//------------------------------------------------------------------------------
/**
������� ������� ������� �� ���-������
\param un -
\param dimensions - �������
\param hotPoints - ���-�����
*/
//--- 
void ShMtBendHook::CreateDimensionsByHotPoints( IfUniqueName * un,
  SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints )
{
  for ( size_t i = 0, count = hotPoints.Count(); i < count; ++i )
  {
    _ASSERT( hotPoints[i] != nullptr );
    switch ( hotPoints[i]->GetIdent() )
    {
    case ehp_ShMtBendDistanceExtr:
      ::AttachGnrLinearDimension( dimensions, *hotPoints[i], ed_length );
      break;
    case ehp_ShMtBendRadius:
      ::AttachGnrRadialDimension( dimensions, *hotPoints[i], ed_radius );
      break;
    case ehp_ShMtBendAngle:
      ::AttachGnrAngularDimension( dimensions, *hotPoints[i], ed_angle );
      break;
    };
  }

  ShMtBaseBendLine::CreateDimensionsByHotPoints( un, dimensions, hotPoints );
}


//------------------------------------------------------------------------------
/**
������������� ������� �������� �������� - ������
\param dimensions - �������
\param children - �������� ����
*/
//--- 
void ShMtBendHook::AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
  ShMtBendObject & children )
{
  ::UpdateDimensionVar( dimensions, ed_childrenRadius, children.m_radius );
}


//------------------------------------------------------------------------------
/**
������� ������� ������� �� ���-������ ��� �������� ��������
\param un -
\param dimensions - �������
\param hotPoints - ���-�����
*/
//--- 
void ShMtBendHook::CreateChildrenDimensionsByHotPoints( IfUniqueName * un,
  SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  ShMtBendObject & children )
{
  for ( size_t i = 0, count = hotPoints.Count(); i < count; ++i )
  {
    _ASSERT( hotPoints[i] != nullptr );
    if ( hotPoints[i]->GetIdent() == ehp_ShMtBendObjectRadius && !children.m_byOwner )
    {
      ::AttachGnrRadialDimension( dimensions, *hotPoints[i], ed_childrenRadius );
    }
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� �������� ������� �� ���-������ ��� �������� ��������
\param dimensions - �������
\param hotPoints - ���-�����
\param children - �������� ����
*/
//--- 
void ShMtBendHook::UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  ShMtBendObject & children )
{
  UpdateRadiusDimension( dimensions, hotPoints, ehp_ShMtBendObjectRadius, ed_childrenRadius, children.m_internal );
}


//------------------------------------------------------------------------------
/**
�������� ��������� �������� ������� �� ���-������
\param dimensions - �������
\param hotPoints - ���-�����
*/
//--- 
void ShMtBendHook::UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  const PArray<OperationSpecificDispositionVariant> * /*pVariants*/ )
{
  if ( !IsValid() )
    return;

  if ( m_straighten )
  {
    ResetRadiusDimension( dimensions, ed_radius );
    ResetLengthDimension( dimensions );
    ResetAngleDimension( dimensions );
  }
  else
  {
    UpdateLengthDimension( dimensions, hotPoints );
    UpdateRadiusDimension( dimensions, hotPoints, ehp_ShMtBendRadius, ed_radius, m_params.m_internal );
    UpdateAngleDimension( dimensions, hotPoints );
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ������
\param dimensions - �������
*/
//--- 
void ShMtBendHook::ResetLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions )
{
  IFPTR( OperationLinearDimension ) lengthDimension( ::FindGnrDimension( dimensions, ed_length, false ) );
  if ( lengthDimension != nullptr )
  {
    lengthDimension->ResetDimensionPosition();
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ������
\param dimensions - �������
*/
//--- 
void ShMtBendHook::ResetAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions )
{
  IFPTR( OperationAngularDimension ) angleDimension( FindGnrDimension( dimensions, ed_angle, false ) );
  if ( angleDimension != nullptr )
  {
    angleDimension->ResetDimensionPosition();
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ �������
\param dimensions - �������
\param radiusDimensionId - id �������
*/
//--- 
void ShMtBendHook::ResetRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions, EDimensionsIds radiusDimensionId )
{
  IFPTR( OperationCircularDimension ) radiusDimension( FindGnrDimension( dimensions, radiusDimensionId, false ) );
  if ( radiusDimension != nullptr )
  {
    radiusDimension->ResetSupportingArc();
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ���� �����
\param pDimensions - ������� ��������
\param pUtils - ��������� ���-�����
*/
//---
void ShMtBendHook::UpdateAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints )
{
  IFPTR( OperationAngularDimension ) angleDimension( FindGnrDimension( dimensions, ed_angle, false ) );
  if ( angleDimension == nullptr )
    return;

  MbCartPoint3D dimensionCenter;
  MbCartPoint3D dimensionPoint1;
  MbCartPoint3D dimensionPoint2;
  bool parametersCalculateSuccess = GetAngleDimensionParameters( hotPoints, dimensionCenter, dimensionPoint1, dimensionPoint2 );

  if ( parametersCalculateSuccess )
  {
    angleDimension->SetDimensionType( ::CalcAngularGnrDimensionType( m_params.GetAngle() * Math::DEGRAD ) );
    angleDimension->SetDimensionPosition( dimensionCenter, dimensionPoint1, dimensionPoint2 );
  }
  else
  {
    angleDimension->ResetDimensionPosition();
  }
}

//------------------------------------------------------------------------------
/**
������� ��������� ������� ����
\param hotPoints - ���-�����
\param dimensionCenter - ����� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
*/
//--- 
bool ShMtBendHook::GetAngleDimensionParameters( const RPArray<HotPointParam> & hotPoints,
  MbCartPoint3D & dimensionCenter,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 )
{
  HotPointBendAngleParam * angleHotPoint = dynamic_cast<HotPointBendAngleParam *>(FindHotPointWithId( ehp_ShMtBendAngle, hotPoints ));
  if ( angleHotPoint == nullptr )
    return false;
  MbCartPoint3D * angleHotPointPosition = angleHotPoint->GetBasePoint();
  if ( angleHotPointPosition == nullptr )
    return false;
  const MbAxis3D * dimensionArcCenterAxis = angleHotPoint->GetAxis();
  if ( dimensionArcCenterAxis == nullptr )
    return false;
  HotPointVectorParam * directionHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( ehp_ShMtBendDirection, hotPoints ));
  if ( directionHotPoint == nullptr )
    return false;

  const MbCartPoint3D & dimensionArcCenter = dimensionArcCenterAxis->GetOrigin();

  MbVector3D anglePointToArcCenter( *angleHotPointPosition, dimensionArcCenter );
  anglePointToArcCenter.Normalize();

  dimensionCenter = dimensionArcCenter;
  dimensionPoint1 = dimensionArcCenter + anglePointToArcCenter;
  if ( m_params.m_typeAngle )
    dimensionPoint2 = dimensionArcCenter + directionHotPoint->m_norm;
  else
    dimensionPoint2 = dimensionArcCenter - directionHotPoint->m_norm;

  return true;
}


//------------------------------------------------------------------------------
/**
�������� ������ �������
\param dimensions - �������
\param hotPoints - ���-�����
\param radiusHotPointId -
\param radiusDimensionId -
\param internal -
\return
*/
//--- 
void ShMtBendHook::UpdateRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  int radiusHotPointId,
  EDimensionsIds radiusDimensionId,
  bool moveToInternal )
{
  IFPTR( OperationCircularDimension ) radiusDimension( FindGnrDimension( dimensions, radiusDimensionId, false ) );
  if ( radiusDimension == nullptr )
    return;

  MbPlacement3D dimensionPlacement;
  MbArc3D dimensionArc( MbPlacement3D(), 0., 0., 0. );
  bool parametersCalculateSuccess = GetRadiusDimensionParameters( dynamic_cast<HotPointBendLineRadiusParam *>(FindHotPointWithId( radiusHotPointId, hotPoints )),
    moveToInternal, dimensionPlacement, dimensionArc );

  if ( m_straighten )
  {
    radiusDimension->ResetSupportingArc();
  }
  else
  {
    radiusDimension->SetDimensionPlacement( dimensionPlacement );
    radiusDimension->SetSupportingArc( dimensionArc );
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� �������
*/
//--- 
bool ShMtBendHook::GetRadiusDimensionParameters( HotPointBendLineRadiusParam * radiusHotPoint, bool moveToInternal,
  MbPlacement3D & dimensionPlacement, MbArc3D & dimensionArc )
{
  if ( radiusHotPoint == nullptr )
    return false;
  if ( radiusHotPoint->GetBasePoint() == nullptr )
    return false;
  if ( radiusHotPoint->m_bendExternalArc.IsDegenerate() )
    return false;

  MbArc3D arc( radiusHotPoint->m_bendExternalArc, radiusHotPoint->m_bendExternalArc.GetTMin(), radiusHotPoint->m_bendExternalArc.GetTMax(), 1 );

  // ����������� �� ���������� ������
  if ( moveToInternal )
  {
    arc.SetRadiusA( arc.GetRadiusA() - m_params.m_thickness );
    arc.SetRadiusB( arc.GetRadiusB() - m_params.m_thickness );
  }

  if ( !arc.IsDegenerate() )
  {
    dimensionPlacement = arc.GetPlacement();
    dimensionArc.Init( arc );
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ������ ����������
\param dimensions - �������
\param hotPoints - ���-�����
*/
//--- 
void ShMtBendHook::UpdateLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints )
{
  MbPlacement3D dimensionPlacement;
  MbCartPoint3D dimensionPoint1;
  MbCartPoint3D dimensionPoint2;
  bool parametersCalculateSuccess = false;

  switch ( m_params.m_typeDimansion )
  {
  case ShMtBendHookParameters::bd_out:
    parametersCalculateSuccess = GetLengthDimensionParametersByOut( hotPoints, dimensionPlacement, dimensionPoint1, dimensionPoint2 );
    break;
  case ShMtBendHookParameters::bd_in:
    parametersCalculateSuccess = GetLengthDimensionParametersByIn( hotPoints, dimensionPlacement, dimensionPoint1, dimensionPoint2 );
    break;
  case ShMtBendHookParameters::bd_all:
    parametersCalculateSuccess = GetLengthDimensionParametersByAll( hotPoints, dimensionPlacement, dimensionPoint1, dimensionPoint2 );
    break;
  default:
    _ASSERT( FALSE );
  };

  IFPTR( OperationLinearDimension ) lengthDimension( ::FindGnrDimension( dimensions, ed_length, false ) );
  if ( lengthDimension != nullptr )
  {
    if ( parametersCalculateSuccess )
    {
      lengthDimension->SetDimensionPlacement( dimensionPlacement, false );
      lengthDimension->SetDimensionPosition( dimensionPoint1, dimensionPoint2 );
    }
    else
    {
      lengthDimension->ResetDimensionPosition();
    }
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ���������� - �������
\param hotPoints - ���-�����
\param dimensionPlacement - ��������� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
*/
//--- 
bool ShMtBendHook::GetLengthDimensionParametersByOut( const RPArray<HotPointParam> & hotPoints,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 )
{
  HotPointVectorParam * lengthHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( ehp_ShMtBendDistanceExtr, hotPoints ));
  if ( lengthHotPoint == nullptr )
    return false;
  MbCartPoint3D * lengthHotPointPosition = lengthHotPoint->GetBasePoint();
  if ( lengthHotPointPosition == nullptr )
    return false;
  HotPointVectorParam * nonMovableHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( ehp_ShMtBendDirection2, hotPoints ));
  if ( nonMovableHotPoint == nullptr )
    return false;

  MbCartPoint3D point1 = *lengthHotPointPosition;
  MbCartPoint3D point2 = *lengthHotPointPosition - lengthHotPoint->m_norm * m_params.m_distanceExtr;
  MbVector3D axisX( point1, point2 );
  MbVector3D axisY( nonMovableHotPoint->m_norm );

  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ���������� - ������
\param hotPoints - ���-�����
\param dimensionPlacement - ��������� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
*/
//--- 
bool ShMtBendHook::GetLengthDimensionParametersByIn( const RPArray<HotPointParam> & hotPoints,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 )
{
  HotPointVectorParam * lengthHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( ehp_ShMtBendDistanceExtr, hotPoints ));
  if ( lengthHotPoint == nullptr )
    return false;
  MbCartPoint3D * lengthHotPointPosition = lengthHotPoint->GetBasePoint();
  if ( lengthHotPointPosition == nullptr )
    return false;
  HotPointVectorParam * nonMovableHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( ehp_ShMtBendDirection2, hotPoints ));
  if ( nonMovableHotPoint == nullptr )
    return false;

  MbCartPoint3D point1 = *lengthHotPointPosition - lengthHotPoint->m_norm * m_params.m_thickness;
  MbCartPoint3D point2 = *lengthHotPointPosition - lengthHotPoint->m_norm * (m_params.m_distanceExtr + m_params.m_thickness);
  MbVector3D axisX( point1, point2 );
  MbVector3D axisY( -nonMovableHotPoint->m_norm );

  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//------------------------------------------------------------------------------
/**
�������� ��������� ������� ���������� - ������
\param hotPoints - ���-�����
\param dimensionPlacement - ��������� �������
\param dimensionPoint1 - ����� 1 �������
\param dimensionPoint2 - ����� 2 �������
*/
//--- 
bool ShMtBendHook::GetLengthDimensionParametersByAll( const RPArray<HotPointParam> & hotPoints,
  MbPlacement3D & dimensionPlacement,
  MbCartPoint3D & dimensionPoint1,
  MbCartPoint3D & dimensionPoint2 )
{
  HotPointVectorParam * lengthHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( ehp_ShMtBendDistanceExtr, hotPoints ));
  if ( lengthHotPoint == nullptr )
    return false;
  MbCartPoint3D * lengthHotPointPosition = lengthHotPoint->GetBasePoint();
  if ( lengthHotPointPosition == nullptr )
    return false;
  HotPointVectorParam * nonMovableHotPoint = dynamic_cast<HotPointVectorParam *>(FindHotPointWithId( ehp_ShMtBendDirection2, hotPoints ));
  if ( nonMovableHotPoint == nullptr )
    return false;

  MbCartPoint3D point1 = *lengthHotPointPosition;
  MbCartPoint3D point2 = *lengthHotPointPosition - lengthHotPoint->m_norm * m_params.m_distanceExtr;
  MbVector3D axisX( point1, point2 );
  MbVector3D axisY( -nonMovableHotPoint->m_norm );

  if ( !axisX.Colinear( axisY, PARAM_EPSILON ) )
  {
    dimensionPlacement.InitXY( point1, axisX, axisY, true );
    dimensionPoint1 = point1;
    dimensionPoint2 = point2;
    return true;
  }
  else
  {
    return false;
  }
}


//-----------------------------------------------------------------------------
// ����������� �������������� ���-�����
/**
\param hotPoints - ���-������
\param children - ����, ��� �������� ���������� ���-�����
*/
//---
void ShMtBendHook::UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  ShMtBendObject &         children )
{
  bool del = true;

  if ( !children.m_byOwner )
  {
    // ���-����� ������� ���������� ������ ���� ���� �������� �� ���������� ������ �����
    for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
    {
      HotPointParam * hotPoint = hotPoints[i];
      if ( hotPoint->GetIdent() == ehp_ShMtBendObjectRadius ) // ������ �����
      {
        MbJogValues * params = nullptr;
        MbBendValues  secondBendParams;

        std::unique_ptr<HotPointBendHookOperUtil> util = GetHotPointUtil( params, secondBendParams,
          hotPoints[0]->GetEditComponent( false/*addref*/ ),
          &children );
        if ( util )
        {
          del = false;

          HotPointBendHookObjectRadiusParam * hotPointBend = (HotPointBendHookObjectRadiusParam *)hotPoint;
          hotPointBend->params = *params;
          hotPointBend->m_deepness = params->displacement;

          if ( children.m_ind % 2 == 0 ) // ������
            util->UpdateRadiusHotPoint_2( *hotPoint, *params, children.m_radius, children.m_internal );
          else
            util->UpdateRadiusHotPoint( *hotPoint, *params, (ShMtBendLineParameters &)GetParameters() );

          hotPointBend->Curves().Flush();
          util->GetPolygonCurves( hotPointBend->Curves(), *params );

          delete params;
        }
      }
    }
  }

  if ( del )
    for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
    {
      hotPoints[i]->SetBasePoint( nullptr );
    }
}
// *** ���-����� ***************************************************************


IMP_PERSISTENT_CLASS( kompasDeprecatedAppID, ShMtBendHook );
IMP_PROC_REGISTRATION( ShMtBendHook );


//--------------------------------------------------------------------------------
/**
���������� �� ���������� ��� �������
\param version
\return bool
*/
//---
bool ShMtBendHook::IsNeedSaveAsUnhistored( long version ) const
{
  return __super::IsSavedUnhistoried( version, m_params.m_typeRemoval ) || __super::IsNeedSaveAsUnhistored( version );
}


//------------------------------------------------------------------------------
// ������ �� ������ 
// ---
void ShMtBendHook::Read( reader &in, ShMtBendHook * obj ) {
  ReadBase( in, static_cast<ShMtBaseBendLine *>(obj) );
  in >> obj->m_upToObject;
  in >> obj->m_params;
}


//------------------------------------------------------------------------------
// ������ � �����
// ---
void ShMtBendHook::Write( writer &out, const ShMtBendHook * obj ) {
  if ( out.AppVersion() < 0x0700010FL )
    out.setState( io::cantWriteObject ); // ���������� ��������� ��� ��������� ��������
  else {
    WriteBase( out, static_cast<const ShMtBaseBendLine *>(obj) );
    out << obj->m_upToObject;
    out << obj->m_params;
  }
}


//------------------------------------------------------------------------------
// ����
// ---
_OBJ01FUNC( IfShMtBend * ) CreateShMtBend( ShMtBendParameters & _pars, IfUniqueName * un )
{
  ShMtBend * pOper;
  try {
    pOper = new ShMtBend( _pars, un );
    pOper->AddRef();
  }
  catch ( ... ) {
    return nullptr;
  }
  return pOper;
}


//------------------------------------------------------------------------------
// ���� �� �����
// ---
_OBJ01FUNC( IfShMtBendLine * ) CreateShMtBendLine( ShMtBendLineParameters & _pars,
  IfUniqueName * un )
{
  ShMtBendLine * pOper;
  try {
    pOper = new ShMtBendLine( _pars, un );
    pOper->AddRef();
  }
  catch ( ... ) {
    return nullptr;
  }
  return pOper;
}


//------------------------------------------------------------------------------
// ��������
// ---
_OBJ01FUNC( IfShMtBendLine * ) CreateShMtBendHook( ShMtBendHookParameters & _pars,
  IfUniqueName * un )
{
  ShMtBendHook * pOper;
  try {
    pOper = new ShMtBendHook( _pars, un );
    pOper->AddRef();
  }
  catch ( ... ) {
    return nullptr;
  }
  return pOper;
}




