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

// построитель сгибов для операции сгиб и сгиб по линии
/*
существует только на сеанс построения/перестроения
работает в PrepareLocalSolid, один для всех заходов по ниткам
*/
static CreatorForBendUnfoldParams * creatorBends = nullptr;
// построитель сгибов для операции подсечка
static CreatorForBendHookUnfoldParams * creatorBendHooks = nullptr;

////////////////////////////////////////////////////////////////////////////////
//
// класса операций сгиб(базовый)
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
// конструктор дублирования
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
// установить параметры по другому объекту
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
/// изменить уникальное имя параметра
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
// Модифицирован ли какой-либо из параметров, связанных с переменными
// ---
bool ShMtBaseBend::IsAnyVariableParameterModified() const
{
  return PartOperation::IsDrivingParametersModified() || IsModifiedAnyBend();
}

//------------------------------------------------------------------------------
// задать кривую сгиба
// ---
void ShMtBaseBend::SetObject( const WrapSomething * wrapper ) {
  m_bendObject.SetObject( 0, wrapper );
}


//------------------------------------------------------------------------------
// получить кривую сгиба
// ---
const WrapSomething & ShMtBaseBend::GetObject() const {
  return m_bendObject.GetObjectTrans( 0 );
}


//------------------------------------------------------------------------------
/**
Объект присутствует среди запомненых
*/
//---
bool ShMtBaseBend::IsObjectExist( const WrapSomething * bend ) const
{
  return bend && m_bendObject.IsObjectExist( *bend );
}


//------------------------------------------------------------------------------
// проверка определенности операции
// ---
bool ShMtBaseBend::IsValid() const {
  return PartOperation::IsValid() && !m_bendObject.IsEmpty() && IsValidBends();
}


//-----------------------------------------------------------------------------
// возможно построение более двух тел ??
//---
bool ShMtBaseBend::IsPossibleMultiSolid() const {
  return true;
}


//------------------------------------------------------------------------------
// восстановить связи
// ---
bool ShMtBaseBend::RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath )
{
  bool res = PartOperation::RestoreReferences( result, allRefs, withMath );
  bool resObj = m_bendObject.RestoreReferences( result, allRefs, withMath );

  return res && resObj;
} // RestoreReferences


  //------------------------------------------------------------------------------
  // Очистить опорные объекты soft = true - только враперы (не трогая имена)
  // ---
void ShMtBaseBend::ClearReferencedObjects( bool soft )
{
  PartOperation::ClearReferencedObjects( soft );
  m_bendObject.ClearObjects( soft );
}


//------------------------------------------------------------------------------
// Заменить ссылки на объекты
// ---
bool ShMtBaseBend::ChangeReferences( ChangeReferencesData & changedRefs )
{
  return m_bendObject.ChangeReferences( changedRefs );
}


//------------------------------------------------------------------------------
// установка параметров и создание Solida для отрисовки фантома
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
// набирает массив предупреждений из ресурса
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
// ключи поиска битмапов в дереве
// ---
void ShMtBaseBend::GetBmpKeys( short int &k1, short int &k2 ) const {
  k1 = ::hash( ::pureName( GetTypeInfo().name() ) );
  k2 = (short int)(IsStraighten() ? 1 : 0);
}


//------------------------------------------------------------------------------
// разбор своих VEF'ов
// ---
void ShMtBaseBend::ResetPrimitives( const IfComponent &compOwner )
{
  PartOperation::ResetPrimitives( compOwner ); // выбрать свои VEF
  InitSheetMetalFaces( compOwner, GetPartOperationResult(), MainId() );

  ResetPrimitivesInSubBends( compOwner );
}


//------------------------------------------------------------------------------
/**
выставить флаг всем примитивам
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
снять владельца всем примитивам
*/
//--- 
void ShMtBaseBend::SetVEFZeroOwner()
{
  PartOperation::SetVEFZeroOwner();
  SetVEFZeroOwnerInSubBends();
}



//------------------------------------------------------------------------------
/**
пересчитать операцию
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
// выдать массив параметров, связанных с переменными
// ---
void ShMtBaseBend::GetVariableParameters( VarParArray & parVars, ParCollType parCollType )
{
  PartOperation::GetVariableParameters( parVars, parCollType );
  ShMtStraighten::GetVariableParameters( parVars, parCollType );
}


//------------------------------------------------------------------------------
// сбросить флаги модифицирования параметров
//---
void ShMtBaseBend::ClearModifiedFlags( VarParArray & array ) // сбросить признаки модификации в параметрах
                                                             //SA K9 31.10.2006 void ShMtBaseBend::ClearModifiedFlags( FDPArray<VarParameter> & array ) 
{
  PartOperation::ClearModifiedFlags( array );
  ShMtStraighten::ClearModifiedFlags( array );
}


//------------------------------------------------------------------------------
// Закончено чтение
// ---
void ShMtBaseBend::AdditionalReadingEnd( PartRebuildResult & lastResult )
{
  PartOperation::AdditionalReadingEnd( lastResult );
  UpdateMeasurePars( gtt_none, true );

  AdditionalReadingEndInSubBends( lastResult );
}


//------------------------------------------------------------------------------
/**
Закочено чтение
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
// Чтение из потока
// ---
void ShMtBaseBend::Read( reader &in, ShMtBaseBend * obj )
{
  ReadBase( in, static_cast<PartOperation *>(obj) );

  if ( in.AppVersion() < 0x11000033L )
  {
    ReferenceContainerFix bendObject( 1 );
    in >> bendObject;
    // Раньше не могло быть более одного сгиба, поэтому очистим все, а потом один прочитанный добавим
    obj->m_bendObject.Reset();
    obj->m_bendObject.Add( bendObject[0]->Duplicate() );
  }
  else
    in >> obj->m_bendObject;

  ReadStraightenOper( in, obj );
  obj->ReadSurf( in, 0x0800011AL );
}


//------------------------------------------------------------------------------
// Запись в поток
// ---
void ShMtBaseBend::Write( writer &out, const ShMtBaseBend * obj ) {
  if ( out.AppVersion() < 0x06000033L )
    out.setState( io::cantWriteObject ); // специально введенный тип ошибочной ситуации
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

    // СС К8+ многотельность
    if ( out.AppVersion() >= 0x0800011AL )
      obj->WriteSurf( out );
  }
}


////////////////////////////////////////////////////////////////////////////////
//
// Класс операции сгиб
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
    tcb_AdjoiningBetween = 0x1, // Замыкание смежных углов
    tcb_AdjoiningBeginEnd = 0x2, // Замыкание вначале и вконце
  };

  typedef uint TypeCircuitBendFlag;

private:
  mutable TypeCircuitBendFlag m_typeCircuitBend;

public:

  ShMtBendParameters     m_params;     ///< Параметры операции
  ReferenceContainerFix  m_refObjects; ///< Ссылки на ассоциативные объекты

public:
  /// Конструктор, просто запоминает параметры ничего не рассчитывая, эскиз дадим потом
  /// Конструктор для использования в конструкторе процесса
  ShMtBend( const ShMtBendParameters &, IfUniqueName * un );
  /// Конструктор копирования
  ShMtBend( const ShMtBend & );

public:
  I_DECLARE_SOMETHING_FUNCS;

  virtual bool                              IsObjectExist( const WrapSomething * bend ) const { return ShMtBaseBend::IsObjectExist( bend ); }
  virtual double                            GetOwnerAbsAngle() const override;  ///
  virtual double                            GetOwnerValueBend() const override;  ///
  virtual double                            GetOwnerAbsRadius() const override;  /// Внутренний радиус сгиба
  virtual bool                              IsOwnerInternalRad() const override;  /// По внутреннему радиусу?
  virtual UnfoldType                        GetOwnerTypeUnfold() const override;  /// Дать тип развертки  
  virtual void                              SetOwnerTypeUnfold( UnfoldType t ) override; /// Задать тип развертки  
  virtual bool                              GetOwnerFlagByBase() const override;  /// 
  virtual void                              SetOwnerFlagByBase( bool val ) override; /// 
  virtual double                            GetOwnerCoefficient() const override;  /// Коэффициент
  virtual bool                              IfOwnerBaseChanged( IfComponent * trans ) override; /// Если изменились какие-либо параметры, и есть чтение коэф из базы, то прочитать базу 
  virtual double                            GetThickness() const override;   /// Дать толщину
  virtual void                              SetThickness( double ) override; /// Задать толщину      

                                                                             //IfShMtCircuitBend
                                                                             /// можно ли использовать разделку смежных углов
  virtual       bool            IsCanoutAdjoiningBetween() const override;
  /// можно ли использовать разделку вначале
  virtual       bool            IsCanoutAdjoiningBegin() override;
  /// можно ли использовать разделку вконце
  virtual       bool            IsCanoutAdjoiningEnd() override;
  virtual       bool            IsClosedPath() const  override;
  virtual ShMtCircuitBendParameters & GetCircuitParameters() override { return m_params; }


  //------------------------------------------------------------------------------
  // IfSheetMetalOperation
  // перед тем как заканчиваем редактирование
  virtual void                              EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp ) override;

  //------------------------------------------------------------------------------
  // IfRefContainersOwner
  virtual void GetRefContainers( FDPArray<AbsReferenceContainer> & ) override;

  //------------------------------------------------------------------------------
  // Реализация IfShMtBend
  virtual void                              SetParameters( const ShMtBaseBendParameters & ) override;
  virtual ShMtBaseBendParameters          & GetParameters() const override;
  virtual void                              SetBendObject( const WrapSomething * ) override;
  virtual const WrapSomething &             GetBendObject() const override;

#ifdef MANY_THICKNESS_BODY_SHMT
  // годится ли пришедший объект
  virtual bool                              IsSuitedObject( const WrapSomething & ) override;
#else
  virtual bool                              IsSuitedObject( const WrapSomething &, double ) override;
#endif

  virtual bool                              IsValid() const override;  // проверка определенности объекта
                                                                       //---------------------------------------------------------------------------
                                                                       // IfCopibleObject
                                                                       // Создать копию объекта
  virtual IfPartObjectCopy * CreateObjectCopy( IfComponent & compOwner ) const override;
  // Следует копировать на указанном компоненте
  virtual bool               NeedCopyOnComp( const IfComponent & component ) const override;
  virtual bool               IsAuxGeom() const override { return false; } ///< Это вспомогательная геометрия?
                                                                          /// Может ли операция менятся по параметрам в массиве
  virtual bool               CanCopyByVarsTable() const override { return false; }

  virtual bool               AddBendObject( const WrapSomething & ) override;
  virtual bool               RemoveBendObject( size_t index ) override;
  virtual void               FlushBendObject() override;
  virtual WrapSomething      GetBend( size_t i ) const override;
  virtual size_t             GetBendCount() const override;
  virtual bool               IsMultiBend() const override;


  //---------------------------------------------------------------------------
  // IfCopibleOperation
  // Можно ли копировать эту операцию как-нибудь иначе, кроме как геометрически?
  // Т.е. через MakePureSolid или через интерфейсы IfCopibleOperationOnPlaces,
  // IfCopibleOperationOnEdges и т.д.
  virtual bool                          CanCopyNonGeometrically() const override;

  //---------------------------------------------------------------------------
  // IfCopibleObjectOnRefs
  // Выдать ссылки на объекты, образы которых нужно найти на некотором расстоянии
  virtual const AbsReferenceContainer * GetReferencesToFind( const IfComponent & comp ) override;

  //---------------------------------------------------------------------------
  // IfCopibleOperationOnEdges
  // Дать внутренние ссылки, которые при копировании должны быть заменены на новые
  virtual bool  GetInternalReferences( AbsReferenceContainer & internalRefs ) override;
  // Построить операцию для найденных ребер.
  virtual uint  MakeSolidOnEdges( MakeSolidParam        & mksdParam,
    const MbPlacement3D   & copyPlace,
    FDPArray<MbCurveEdge> & foundEdges,
    EdgesPairs        *     internalEdgesPairs,
    SimpleName              mainId,
    bool                    needInvert,
    MbSolid              *& resultSolid,
    PArray<MbPositionData> * posData ) override;

  //------------------------------------------------------------------------------
  // общие функции объекта детали 
  virtual uint            GetObjType() const override;                            // вернуть тип объекта
  virtual uint            GetObjClass() const override;                            // вернуть общий тип объекта
  virtual uint            OperationSubType() const override;   // вернуть подтип операции
  virtual void            Assign( const PrObject &other ) override;           // установить параметры по другому объекту
  virtual PrObject      & Duplicate() const override;

  virtual void            WhatsWrong( WarningsArray & ); // набирает массив предупреждений из ресурса
  virtual void            GetVariableParameters( VarParArray &, ParCollType parCollType ) override;
  virtual void            ForgetVariables(); // ИР K6+ забыть все переменные, связанные с параметрами
  virtual void            AdditionalReadingEnd( PartRebuildResult & prr ) override; /// После вызова ReadingEnd
                                                                                    /// восстановить связи
  virtual bool            RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath ) override;
  virtual void            ClearReferencedObjects( bool soft ) override;
  virtual bool            ChangeReferences( ChangeReferencesData & changedRefs ) override; ///< Заменить ссылки на объекты
  virtual bool            IsThisParent( uint8 relType, const WrapSomething & ) const override;
  /// изменить уникальное имя параметра
  virtual void            RenameUnique( IfUniqueName & un ) override;

  virtual bool            PrepareToAdding( PartRebuildResult &, PartObject * ) override; //подготовиться ко встраиванию в модель
  virtual bool            InitPhantom( const IfComponent &, bool /*setNames*/ ) override;
  // создание фантомного или результативного Solid
  // код ошибки необходимо обязательно проинициализировать
  virtual MbSolid       * MakeSolid( uint              & codeError,
    MakeSolidParam    & mksdParam,
    uint                shellThreadId ) override;

  virtual void            PrepareToBuild( const IfComponent & ) override; ///< подготовиться к построению
  virtual void            PostBuild() override; ///< некие действия после построения
  virtual bool            CopyInnerProps( const IfPartObject& source,
    uint sourceSubObjectIndex,
    uint destSubObjectIndex ) override;

  /// Подготовка фантомного или результативного тела
  MbSolid       * PrepareLocalSolid( std::vector<MbCurveEdge*> edges,
    uint              & codeError,
    MakeSolidParam    & mksdParam,
    uint                shellThreadId,
    SimpleName          mainId );
  /// Годится ли пришедший объект
  bool            IsSuitedObject( const MbCurveEdge &, double );
  /// Надо ли сохранять как объект без истории
  virtual bool            IsNeedSaveAsUnhistored( long version ) const override;
  virtual void            ConvertTo5_11( IfCanConvert & owner, const WritingParms & wrParms ) override;
  virtual void            ConvertToSavePrevious( SaveDocumentVersion saveMode, IfCanConvert & owner, const WritingParms & wrParms ) override;
  virtual IfCanConvert *  CreateConverter( IfCanConvert & what ) override;
  virtual bool            IsNeedConvertTo5_11( SSArray<uint> & types, IfCanConvert & owner ) override;
  virtual bool            IsNeedConvertToSavePrevious( SSArray<uint> & types, SaveDocumentVersion saveMode, IfCanConvert & owner ) override;

  /// Возможно ли для параметра задавать допуск
  virtual bool            IsCanSetTolerance( const VarParameter & varParameter ) const override;
  /// Для параметра получить неуказанный предельный допуск
  virtual double          GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const override;
  /// Является ли параметр угловым
  virtual bool            ParameterIsAngular( const VarParameter & varParameter ) const override;

  // Реализация IfHotPoints
  /// Создать хот-точки
  virtual void GetHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool ) override;
  /// Пересчитать местоположение хот-точки
  virtual void UpdateHotPoints( RPArray<HotPointParam> & hotPoints,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool ) override;
  /// Начало перетаскивания хот-точки (на хот-точке нажата левая кнопка мыши)
  virtual bool BeginDragHotPoint( HotPointParam & hotPoint ) override;
  /// Перерасчет при перетаскивании хот-точки 
  // step - шаг дискретности, для определения смещения с заданным пользователем шагом
  virtual bool ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
    double step, D3Draw & pD3DrawTool ) override;
  /// Завершение перетаскивания хот-точки (на хот-точке отпущена левая кнопка мыши)
  virtual bool EndDragHotPoint( HotPointParam & hotPoint, D3Draw & pD3DrawTool ) override;

  // Реализация хот-точек радиусов сгибов от ShMtStraighten
  /// Создать хот-точки (зарезервированы индексы с 5000 до 5999)
  virtual void GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ShMtBendObject &         children ) override; // #84586
                                                  /// Пересчитать местоположение хот-точек
  virtual void UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    ShMtBendObject &         children ) override;
  /// Перерасчет при перетаскивании хот-точки
  virtual bool ChangeChildrenHotPoint( HotPointParam &  hotPoint,
    const MbVector3D &     vector,
    double           step,
    ShMtBendObject & children ) override;

  /// могут ли к объекту быть привязаны управляющие размеры
  virtual bool IsDimensionsAllowed() const override;
  virtual void GetHotPointsForDimensions( RPArray<HotPointParam> & hotPoints,
    IfComponent & editComponent,
    IfComponent & topComponent,
    bool allHotPoints = false ) override;
  /// ассоциировать размеры с переменными родителя
  virtual void AssignDimensionsVariables( const SFDPArray<PartObject> & dimensions ) override;
  /// создать размеры объекта по хот-точкам
  virtual void CreateDimensionsByHotPoints( IfUniqueName * un,
    SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints ) override;
  /// обновить геометрию размеров объекта по хот-точкам
  virtual void UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    const PArray<OperationSpecificDispositionVariant>* variants ) override;

  /// Ассоциировать размеры дочерних объектов - сгибов
  virtual void AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
    ShMtBendObject & children ) override;
  /// создать размеры объекта по хот-точкам для дочерних объектов
  virtual void CreateChildrenDimensionsByHotPoints( IfUniqueName * un,
    SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children ) override;
  /// обновить геометрию размеров объекта по хот-точкам для дочерних объектов
  virtual void UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children ) override;

  /// Расчет дуги для пересчета угловых параметров по допуску
  virtual void UpdateMeasurePars( GeneralToleranceType gType, bool recalc ) override;


  /// Задать опорный объект
  virtual void SetRefObject( const WrapSomething * wrapper, size_t index ) override;

  /// Получить опорный объект
  virtual const WrapSomething & GetRefObject( size_t index ) const override;

  /// Проверить опорный объект (как вершину)
  virtual const bool IsObjectSuitableAsBaseVertex( const WrapSomething & pointWrapper ) override;

  /// Проверить опорный объект (как плоскость)
  virtual const bool IsObjectSuitableAsBasePlane( const WrapSomething & planeWrapper, bool forLeftSide ) override;

  /// Исправить параметр длины стороны продолжения сгиба
  virtual void FixAbsSideLength( bool isLeftSide ) override;

protected:
  virtual const AbsReferenceContainer * GetRefContainer( const IfComponent & bodyOwner ) const override { return &m_bendObject; }

private:
  bool IsCanoutAdjoining() const; ///< Можно ли использовать замыкание углов вконце и в начале
  void DetermineTypeBens() const;
  /// Получить местоположение объекта относительно плоскости сгиба
  MbeItemLocation BendObjectLocation( const WrapSomething & pointWrapper, bool left );
  bool IsNeedInvertDirectionBend();

  /// Проверка выбранного ребра (KOMPAS-17824: не давать выбирать два ребра с одной торцевой грани)
  bool VerifyNonOppositeEdge( const WrapSomething & bend, const MbCurveEdge & curveEdge ) const;

  /// Проверить, выбраны ли опорные объекты для неабсолютных длины сторон сгиба
  bool IsValidBaseObjects();
  /// Обнулить размер радиуса
  void ResetRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions, EDimensionsIds dimensionId );
  /// Обнулить размер угла
  void ResetAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions );
  /// Получить длину внутренней дуги сгиба
  double GetBendInternalLength( const HotPointBendOperUtil & utils ) const;
  /// Получить специальный класс для расчета побочных параметров операции
  HotPointBendOperUtil * GetHotPointUtil( MbBendByEdgeValues & params,
    IfComponent        & editComponent,
    ShMtBendObject     * children,
    bool ignoreStraightMode = false );
  /// Получить специальный класс для расчета побочных параметров операции
  HotPointBendOperUtil * GetHotPointUtil( MbBendByEdgeValues & params,
    IfComponent * editComponent,
    ShMtBendObject * children,
    bool ignoreStraightMode = false );

  /// Обновить размер радиуса сгиба
  void UpdateRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    EDimensionsIds dimensionId,
    bool moveToExternal );
  /// Обновить размер угла сгиба
  void UpdateAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils ) const;
  /// Обновить размер длины сгиба
  void UpdateLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils ) const;
  /// Обновить размер длины сгиба слева/справа
  void UpdateSideLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    bool isLeftSide ) const;
  /// Обновить размер смещения относительно опорного объекта для продолжения сгиба слева/справа
  void UpdateSideObjectOffsetDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    bool isLeftSide ) const;
  /// Обновить размер смещение сгиба
  void UpdateDeepnessDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils ) const;
  /// Обновить размер отступа сгиба
  void UpdateIndentDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    const ShMtBendHotPoints hotPointId,
    const EDimensionsIds dimensionId,
    double indent ) const;
  /// Обновить размер ширины сгиба
  void UpdateWidthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints ) const;
  /// Обновить размер расширения сгиба
  void UpdateWideningDimensions( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints );
  /// Обновить размеры уклона продолжения сгиба
  void UpdateDeviationDimensions( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints ) const;
  /// Обновить размер освобождения ширины
  void UpdateWidthReleaseDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints );
  /// Обновить размер освобождения - глубина
  void UpdateDepthReleaseDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints );
  /// Обновить рамеры угла уклона сторон сгиба
  void UpdateSideAngleDimensions( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints ) const;
  /// Получить параметры размера длины по продолжению
  bool GetLengthDimensionParametersByContinue( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размера длины по контору
  bool GetLengthDimensionParametersByContour( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размера длины по касанию
  bool GetLengthDimensionParametersByTouch( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размера длины по касанию при угле больше 90
  bool GetLengthDimensionObtuseAngle( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размера длины по касанию при угле больше 90 внутри сгиба
  void GetLengthDimensionObtuseAngleInternal( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размера длины по касанию при угле больше 90 снаружи
  void GetLengthDimensionObtuseAngleNotInternal( const HotPointBendOperUtil & utils,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размеры Угол, тип дополняющий
  bool GetAngleDimensionSupplementary( const HotPointBendOperUtil & utils,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размеры Угол, тип - угол сгиба
  bool GetAngleDimensionBend( const HotPointBendOperUtil & utils,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размера уклона продолжения
  bool GetDeviationDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendHotPoints deviationHotPointId,
    ShMtBendHotPoints sideAngleHotPointId,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размера освобождения ширины
  bool GetWidthReleaseDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// Получить параметры размера освобождения глубины
  bool GetDepthReleaseDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// Получить параметры размера угла уклона стороны сгиба
  bool GetSideAngleDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    double dimensionLength,
    ShMtBendHotPoints hotPointId,
    double angle,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 ) const;
  /// Получить параметры размера расширения сгиба
  bool GetWideningDimensionParameters( const HotPointBendOperUtil & utils,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendHotPoints wideningHotPointId,
    double widening,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// Получить параметры размера продолжения сгиба слева/справа
  bool GetSideLengthDimensionParametersByContinue( const HotPointBendOperUtil & utils,
    MbPlacement3D        & dimensionPlacement,
    MbCartPoint3D        & dimensionPoint1,
    MbCartPoint3D        & dimensionPoint2,
    bool                   isLeftSide ) const;
  /// Получить параметры размера продолжения сгиба слева/справа
  bool GetSideLengthDimensionParametersByContour( const HotPointBendOperUtil & utils,
    MbPlacement3D        & dimensionPlacement,
    MbCartPoint3D        & dimensionPoint1,
    MbCartPoint3D        & dimensionPoint2,
    bool                   isInternal,
    bool                   isLeftSide ) const;
  /// Получить параметры размера продолжения сгиба слева/справа
  bool GetSideLengthDimensionParametersByTouch( const HotPointBendOperUtil & utils,
    MbPlacement3D        & dimensionPlacement,
    MbCartPoint3D        & dimensionPoint1,
    MbCartPoint3D        & dimensionPoint2,
    bool                   isInternal,
    bool                   isLeftSide ) const;
  /// Получить параметры размера смещения относительно опорного объекта для продолжения сгиба слева/справа
  bool GetSideObjectOffsetDimensionParameters( const HotPointBendOperUtil & utils,
    MbPlacement3D        & dimensionPlacement,
    MbCartPoint3D        & dimensionPoint1,
    MbCartPoint3D        & dimensionPoint2,
    bool                   isLeftSide ) const;
  /// Посчитать длину продолжения сгиба до опорного объекта
  const bool CalculateSideLengthToObject( HotPointBendOperUtil & utils,
    bool                   isLeftSide,
    double               & length );
  /// Получить касательную к дуге в точке
  void GetArcTangent( const MbArc3D       & supportArc,
    const MbCartPoint3D & pointOnArc,
    MbLine3D      & tangent ) const;
  /// Пересчитать длину сгиба, если мы строим до вершины/до плоскости
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
  void operator = ( const ShMtBend & ); // не реализовано

  DECLARE_PERSISTENT_CLASS( ShMtBend )
};


//--------------------------------------------------------------------------------
/**
Конструктор, просто запоминает параметры ничего не рассчитывая, эскиз дадим потом.
Конструктор для использования в конструкторе процесса.
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
Конструктор копирования
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
Конструктор
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
Получение интерфейса
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
Создать копию объекта - копию условного изображнния резьбы
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
Следует копировать на указанном компоненте
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
Можно ли копировать эту операцию как-нибудь иначе, кроме как геометрически?
Т.е. через MakePureSolid или через интерфейсы IfCopibleOperationOnPlaces,
IfCopibleOperationOnEdges и т.д.
\return bool
*/
//---
bool ShMtBend::CanCopyNonGeometrically() const
{
  return true;
}


//--------------------------------------------------------------------------------
/**
Выдать ссылки на объекты, образы которых нужно найти на некотором расстоянии
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
Восстановить связи
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
    // для версий старше 11 надо проставить ид нитки в подсгибы
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
Очистить опорные объекты soft = true - только враперы (не трогая имена)
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
Заменить ссылки на объекты
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
Инициализация фантома
\param operOwner
\param setNames
\return bool
*/
//---
bool ShMtBend::InitPhantom( const IfComponent &operOwner, bool setNames )
{
  return InitPhantomPrevSolid( operOwner, tct_Created | tct_Merged | tct_Truncated/*changed*/, true /*false*/, true );  // KVT Все фантомы должны быть проименованы
}


//--------------------------------------------------------------------------------
/**
Дать внутренние ссылки, которые при копировании должны быть заменены на новые
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
Построить операцию для найденных ребер.
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
  MbSolid * result = nullptr; // Результат

  std::vector<MbCurveEdge*> edges;
  for ( size_t i = 0, count = m_bendObject.Count(); i < count; ++i )
  {
    IfSomething * obj = m_bendObject.GetObjectTrans( i ).obj;
    IFPTR( PartEdge ) partEdge( obj );
    // Есть ребро, а так же опорные объекты, если сгиб строится до вершины или плоскости
    if ( !!partEdge && IsValidBaseObjects() )
      if ( MbCurveEdge * curveEdge = partEdge->GetMathEdge() )
        edges.push_back( curveEdge );
  }


  // Сделать сгиб на найденном ребре
  if ( edges.size() )
  {
    result = PrepareLocalSolid( edges, res, mksdParam, -1, mainId );
  }
  else
    res = rt_NoEdges; // ошибка


  if ( result )
    resultSolid = result;

  return res;
}


uint ShMtBend::GetObjType()  const { return ot_ShMtBend; } // вернуть тип объекта
uint ShMtBend::GetObjClass() const { return oc_ShMtBend; } // вернуть общий тип объекта
uint ShMtBend::OperationSubType() const { return bo_Union; } // вернуть подтип операции


                                                             //--------------------------------------------------------------------------------
                                                             /**
                                                             Скопировать объект
                                                             \return PrObject &
                                                             */
                                                             //---
PrObject & ShMtBend::Duplicate() const
{
  return *new ShMtBend( *this );
}


//--------------------------------------------------------------------------------
/**
Установить параметры по другому объекту
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
Задать параметры
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
Получить параметры
\return ShMtBaseBendParameters &
*/
//---
ShMtBaseBendParameters & ShMtBend::GetParameters() const
{
  return const_cast<ShMtBendParameters&>(m_params);
}


//--------------------------------------------------------------------------------
/**
Задать кривую сгиба
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
Получить кривую сгиба
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
Годится ли пришедший объект
\param wrapper
\return bool
*/
//---
bool ShMtBend::IsSuitedObject( const WrapSomething & wrapper )
{
  bool res = false;

  // ША K11 17.12.2008 Ошибка №37142: Запрещаем выбор ребер многочастных тел
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
Годится ли пришедший объект
\param wrapper
\param thickness
\return bool
*/
//---
bool ShMtBend::IsSuitedObject( const WrapSomething & wrapper, double thickness )
{
  bool res = false;

  // ША K11 17.12.2008 Ошибка №37142: Запрещяем выбор ребер многочастных тел
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
Проверка выбранного ребра (KOMPAS-17824: не давать выбирать два ребра с одной торцевой грани)
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
    oldFaceMinus = oldCurveEdge->GetFaceMinus();  // KOMPAS-17824: не допустим выбор ребра,
    oldSheetFace = FindSheetFace( *oldCurveEdge );// имеющего общую грань с каким-либо ранее выбранным ребром
                                                  // и лежащего на противоположной от него грани листового тела
    res = !(oldFacePlus && oldFaceMinus && oldSheetFace && newOppositeFace && newFacePlus && newFaceMinus
      && (newFacePlus == oldFacePlus || newFacePlus == oldFaceMinus || newFaceMinus == oldFacePlus || newFaceMinus == oldFaceMinus)
      && (oldSheetFace == newOppositeFace));
  }

  return res;
}


//--------------------------------------------------------------------------------
/**
TODO: Описание
\return double
*/
//---
double ShMtBend::GetThickness() const
{
  return m_params.m_thickness;    // толщина
}


//--------------------------------------------------------------------------------
/**
TODO: Описание
\param h
\return void
*/
//---
void ShMtBend::SetThickness( double h )
{
  m_params.m_thickness.ForgetVariable();
  m_params.m_thickness = h;    // толщина
}


//--------------------------------------------------------------------------------
/**
TODO: Описание
\return double
*/
//---
double ShMtBend::GetOwnerAbsRadius() const
{
  //  return m_params.GetRadius();
  return m_params.m_internal ? m_params.GetRadius() : (m_params.GetRadius() - GetThickness());    // внутренний радиус сгиба
}


//--------------------------------------------------------------------------------
/**
Коэффициент, определяющий положение нейтрального слоя
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
TODO: Описание
\return double
*/
//---
double ShMtBend::GetOwnerAbsAngle() const
{
  return  m_params.GetAbsAngle();
}


//--------------------------------------------------------------------------------
/**
По внутреннему радиусу
\return bool
*/
//---
bool ShMtBend::IsOwnerInternalRad() const {
  return m_params.m_internal;
}


//--------------------------------------------------------------------------------
/**
Дать тип развертки
\return UnfoldType
*/
//---
UnfoldType ShMtBend::GetOwnerTypeUnfold() const {
  return m_params.m_typeUnfold;
}


//--------------------------------------------------------------------------------
/**
Задать тип развертки
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
TODO: Описание
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
TODO: Описание
\return bool
*/
//---
bool ShMtBend::GetOwnerFlagByBase() const
{
  return m_params.GetFlagUnfoldByBase();
}


//--------------------------------------------------------------------------------
/**
Если изменились какие либо параметры, и есть чтение коэф из базы, то прочитать базу
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
Получить коэффициент
\return double
*/
//---
double ShMtBend::GetOwnerCoefficient() const
{
  return m_params.GetCoefficient();
}


//--------------------------------------------------------------------------------
/**
Определить все доступные типы замыкания углов.
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
Замкнутый ли путь.
*/
//---
bool ShMtBend::IsClosedPath() const
{
  return !IsCanoutAdjoining();
}


//--------------------------------------------------------------------------------
/**
Можно ли использовать замыкание смежных углов.
*/
//---
bool ShMtBend::IsCanoutAdjoiningBetween() const
{
  return !!(m_typeCircuitBend & TypeCircuitBend::tcb_AdjoiningBetween);
}


//------------------------------------------------------------------------------
/**
Можно ли использовать замыкание углов вконце и в начале.
*/
//---
bool ShMtBend::IsCanoutAdjoining() const
{
  return !!(m_typeCircuitBend & TypeCircuitBend::tcb_AdjoiningBeginEnd);
}


//--------------------------------------------------------------------------------
/**
Можно ли использовать замыкание углов вначале.
*/
//---
bool ShMtBend::IsCanoutAdjoiningBegin()
{
  return IsCanoutAdjoining();
}


//--------------------------------------------------------------------------------
/**
Можно ли использовать замыкание углов вконце.
*/
//---
bool ShMtBend::IsCanoutAdjoiningEnd()
{
  return IsCanoutAdjoining();
}


//--------------------------------------------------------------------------------
/**
Перед тем как заканчиваем редактирование
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
Создание фантомного или результативного тела
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
      // Есть ребро, а так же опорные объекты, если сгиб строится до вершины или плоскости
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
  //       // Есть ребро, а так же опорные объекты, если сгиб строится до вершины или плоскости
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
Добавление(удаление если такой уже есть) ребра для построения сгиба.
\return bool добавился или удалился.
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
Удаление ребра по индексу.
\return bool удалось удалить или такого индекса нет.
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
Очистить все ребра сгибв.
*/
//---
void ShMtBend::FlushBendObject()
{
  m_bendObject.Reset();
}


//------------------------------------------------------------------------------
/**
Получить ребро для построения сгиба по индексу
*/
//---
WrapSomething ShMtBend::GetBend( size_t i ) const
{
  return m_bendObject.GetObjectTrans( i );
}


//------------------------------------------------------------------------------
/**
Получить количество ребер для построения сгиба.
\return size_t количество ребер участвующих в построении сгиба.
*/
//---
size_t ShMtBend::GetBendCount() const
{
  return m_bendObject.Count();
}


//------------------------------------------------------------------------------
/**
Построен ли сгиб по нескольким ребрам.
*/
//---
bool ShMtBend::IsMultiBend() const
{
  return m_bendObject.Count() > 1;
}


//--------------------------------------------------------------------------------
/**
Проверка целостности операции
\return bool
*/
//---
bool ShMtBend::IsValid() const
{
  return ShMtBaseBend::IsValid() && m_params.IsValidParam() && m_bendObject.IsFull();
}


//--------------------------------------------------------------------------------
/**
Набирает массив предупреждений из ресурса
\param warnings
\return void
*/
//---
void ShMtBend::WhatsWrong( WarningsArray & warnings )
{
  ShMtBaseBend::WhatsWrong( warnings );

  if ( !m_bendObject.IsFull() )
  {
    warnings.AddWarning( IDP_WARNING_LOSE_SUPPORT_OBJ, module ); // "Потерян один или несколько опорных объектов"
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

  // Проверка на корректное значение параметра длины продолжения сгиба (для задания по вершине и через все)
  if ( (m_params.m_sideLeft.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute &&
    m_params.m_sideLeft.m_length.GetVarValue() < 0) ||
    (m_params.m_bendLengthBy2Sides && m_params.m_sideRight.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute &&
      m_params.m_sideRight.m_length.GetVarValue() < 0) )
    warnings.AddWarning( IDP_RT_PARAMETERERROR, module );

  m_params.WhatsWrongParam( warnings );
}


//--------------------------------------------------------------------------------
/**
Подготовиться ко встраиванию в модель
\param result
\param toObj
\return bool
*/
//---
bool ShMtBend::PrepareToAdding( PartRebuildResult & result, PartObject * toObj )
{
  if ( ShMtBaseBend::PrepareToAdding( result, toObj ) )
  {
    if ( !GetFlagValue( O3D_IsReading ) ) // не прочитан из памяти, а создан как new  BUG 68896
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
Выдать массив параметров, связанных с переменными
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
Забыть все переменные, связанные с параметрами
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
Изменить уникальное имя параметра
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
Подготовиться к построению
\param operOwner
\return void
*/
//---
void ShMtBend::PrepareToBuild( const IfComponent & operOwner ) // компонент - владелец этой операции
{
  creatorBends = new CreatorForBendUnfoldParams( *this, (IfComponent*)&operOwner );
}


//--------------------------------------------------------------------------------
/**
Действия после перестроения
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
Скопировать свойства объекта
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
Подготовка фантомного или результативного тела
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

      m_params.ReadBaseIfChanged( const_cast<IfComponent*>(mksdParam.m_operOwner) ); // если параметры развертки брать из базы, то перечитать базу, если надо
      m_params.GetValues( params, edges[0]->GetMetricLength(), GetObjVersion(), edges.size() > 1 ? ShMtBendParameters::bd_allLength : m_params.m_bendDisposal );
      // В старых версиях неправильно рассчитывалось смещение
      if ( GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0800000BL )
        // Смещение посчитать после всех вычислений
        m_params.CalcAfterValues( params );

      SArray<uint> errors;

      for ( MbCurveEdge * edge : edges )
      {
        UtilBendError bennErrors( params, GetThickness(), m_params, *edge );
        bennErrors.GetErrors( errors );
      }

      // Ошибки, которые можно идентифицировать на этом уровне
      if ( errors.Count() == 0 )
      {
        // Если хотя бы одна из длин считается не в абсолютном виде, то ее надо пересчитать
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
        // в старых версиях неправильно рассчитывалось смещение
        if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
          // Смещение посчитать после всех вычислений
          m_params.CalcAfterValues( params );

        // Если хотя бы одна из длин считается не в абсолютном виде, то ее надо пересчитать
        bool suitableParamValues = true;
        if ( m_params.m_sideLeft.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute ||
          (m_params.m_sideRight.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute && m_params.m_bendLengthBy2Sides) )
          suitableParamValues = UpdateSideLengthsForNonAbsoluteCalcMode( params );

        if ( IsValidParamBaseTable() && suitableParamValues )
        {
          RPArray<MbCurveEdge> curveEdges( 1, INDEX_TO_UINT( edges.size() ) );
          for ( MbCurveEdge * edge : edges )
            curveEdges.Add( edge );

          codeError = BendSheetSolidByEdges( *mksdParam.m_prevSolid/*mksdParam.m_prevSolid*/,     // исходное тело
            mksdParam.m_wDone == wd_Phantom ? cm_Copy : cm_KeepHistory,  // работать с копией оболочки исходного тела
            curveEdges,                // рёбра сгибов
            IsStraighten(),            // гнуть
            params,                    // параметры сгиба 
            pComplexName,              // именователь
            creatorBends->m_mbPars,    // параметры каждого из подсгибов, чтобы сюда математика положила внутренние и внешние плоскости сгиба
            result );                  // результирующее тело

          if ( mksdParam.m_wDone != wd_Phantom )
            // надо запомнить все сгибы и их радиуса
            creatorBends->SetMultiResult( result );

          if ( mksdParam.m_wDone != wd_Phantom && codeError == rt_SelfIntersect )
            codeError = rt_Success;

          if ( mksdParam.m_wDone != wd_Phantom && codeError == rt_Success && result )
            if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x07000110L )
              ::CheckIndexCutedBendFaces( *result, MainId() ); // для правильной идентификации граней сгиба
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
Годится ли пришедший объект как ребро
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
    // Угол между гранями должен быть 90
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
Возможно ли для параметра задавать допуск
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
// Является ли параметр угловым
// ---
bool ShMtBend::ParameterIsAngular( const VarParameter & varParameter ) const
{
  if ( &varParameter == &const_cast<ShMtBendParameters&>(m_params).GetAngleVar() ||
    &varParameter == &m_params.m_sideLeft.m_deviation || // угол уклона продолжения сгиба
    &varParameter == &m_params.m_sideRight.m_deviation ||
    &varParameter == &m_params.m_sideLeft.m_angle || // угол уклона края сгиба 
    &varParameter == &m_params.m_sideRight.m_angle )
    return  true;

  return false;
}


//--------------------------------------------------------------------------------
/**
Для параметра получить неуказанный предельный допуск
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
    // CC K14 Для получения допуска используем радиус. Согласовано с Булгаковым
    return reader.GetTolerance( m_params.m_distance, // внешний радиус BUG 62631
      vd_angle,
      tType );
  }
  else if ( &varParameter == &m_params.m_sideLeft.m_deviation || // угол уклона продолжения сгиба
    &varParameter == &m_params.m_sideRight.m_deviation )
  {
    if ( m_params.m_sideLeft.m_length < Math::paramEpsilon ||
      ::fabs( varParameter.GetDoubleValue() ) < Math::paramEpsilon )
      return 0.0;
    // VB С двумя сторонами абсолютно непонятно, что же здесь должно быть
    return reader.GetTolerance( m_params.m_sideLeft.m_length, vd_angle, tType );
  }
  else if ( &varParameter == &m_params.m_sideLeft.m_angle || // угол уклона края сгиба 
    &varParameter == &m_params.m_sideRight.m_angle )
  {
    if ( ::fabs( varParameter.GetDoubleValue() ) < Math::paramEpsilon )
      return 0.0;

    double angle = ::fabs( m_params.m_typeAngle ? m_params.GetAngle() : 180 - m_params.GetAngle() );
    return reader.GetTolerance( M_PI * m_params.m_distance * angle / 180.0, // длина дуги
      vd_angle,
      tType );
  }


  return ShMtBaseBend::GetGeneralTolerance( varParameter, tType, reader );
}


//------------------------------------------------------------------------------------
/// Расчет дуги для пересчета угловых параметров по допуску
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
    // У сгибов брать наименьший радиус
    double rad = MB_MAXDOUBLE;
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
    {
      double rad_i = m_bendObjects[i]->GetAbsRadius();
      if ( rad_i < rad )
        rad = rad_i;
    }

    m_params.m_distance = rad + GetThickness(); // внешний радиус BUG 62631
  }

  VariableParametersOwner::UpdateMeasurePars( gType, recalc );
}


// *** Хот-точки ***************************************************************
//------------------------------------------------------------------------------
// Создать хот-точки
// ---
void ShMtBend::GetHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  if ( type == ActiveHotPoints::hp_none || type == ActiveHotPoints::hp_param )
  {
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendDeepness, editComponent, topComponent, HotPointVectorParam::ht_moveInvert ) ); // смещение сгиба 
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendLeftSideLength, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // продолжение сгиба слева
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendRightSideLength, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // продолжение сгиба справа
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendLeftSideObjectOffset, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // смещение для длины относительно объекта слева
    hotPoints.Add( new HotPointPlacementParam( ehp_ShMtBendRightSideObjectOffset, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // смещение для длины относительно объекта справа

    hotPoints.Add( new HotPointBendAngleParam( ehp_ShMtBendAngle, editComponent, topComponent ) ); // угол сгиба
    hotPoints.Add( new HotPointVectorParam( ehp_ShMtBendSideLeftDistance, editComponent, topComponent, HotPointVectorParam::ht_moveInvert ) ); // отступ от края сгиба левой стороны
    hotPoints.Add( new HotPointVectorParam( ehp_ShMtBendSideRightDistance, editComponent, topComponent, HotPointVectorParam::ht_moveInvert ) ); // отступ от края сгиба правой стороны
    hotPoints.Add( new HotPointVectorParam( ehp_ShMtBendWidth, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // ширина
    hotPoints.Add( new HotPointDirectionParam( ehp_ShMtBendDirection, editComponent, topComponent ) );
    hotPoints.Add( new HotPointBendRadiusParam( ehp_ShMtBendRadius, editComponent, topComponent ) ); // радиус сгиба 
  }
  if ( !GetCircuitParameters().IsAnyCreateParam() && (type == ActiveHotPoints::hp_none || type == ActiveHotPoints::hp_additional) )
  {
    hotPoints.Add( new HotPointVectorParam( ehp_ShMtBendSideLeftWidening, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // расширение  продолжения (прямой части) сгиба левой стороны
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideRightWidening, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // расширение  продолжения (прямой части) сгиба правой стороны
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideLeftAngle, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // угол уклона края сгиба левой стороны
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideRightAngle, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // угол уклона края сгиба правой стороны
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideLeftDeviation, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // угол уклона продолжения (прямой части) сгиба левой стороны
    hotPoints.Add( new HotPointBendParam( ehp_ShMtBendSideRightDeviation, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // угол уклона продолжения (прямой части) сгиба правой стороны
  }
  if ( type == ActiveHotPoints::hp_none || type == ActiveHotPoints::hp_thinwall )
  {
    hotPoints.Add( new ArrowHeadPointParam( ehp_ShMtBendWidthBend, editComponent, topComponent, HotPointVectorParam::ht_move ) );     // ширина  разгрузки сгиба
    hotPoints.Add( new ArrowHeadPointParam( ehp_ShMtBendDepthBend, editComponent, topComponent/*, HotPointVectorParam::ht_move*/ ) ); // глубина разгрузки сгиба
  }
}


//------------------------------------------------------------------------------
/**
Пересчитать местоположение хот-точки
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
      // Есть ребро, установим опорные объекты (их может не быть)
      util->SetLengthBaseObjects( &m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_leftSideLengthBaseObject ),
        &m_refObjects.GetObjectTrans( IfShMtBend::RefObjIdxs::roi_rightSideLengthBaseObject ) );

      for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
      {
        HotPointParam * hotPoint = hotPoints[i];

        switch ( hotPoint->GetIdent() )
        {
        case ehp_ShMtBendDeepness: util->UpdateDeepnessHotPoint( *hotPoint, params, m_params ); break; // смещение сгиба 
        case ehp_ShMtBendLeftSideLength: util->UpdateSideLengthHotPoint( *hotPoint, params, m_params, ShMtBendSide::LEFT_SIDE ); break; // продолжение сгиба слева
        case ehp_ShMtBendRightSideLength: util->UpdateSideLengthHotPoint( *hotPoint, params, m_params, ShMtBendSide::RIGHT_SIDE ); break; // продолжение сгиба справа
        case ehp_ShMtBendLeftSideObjectOffset: util->UpdateSideObjectOffsetHotPoint( *hotPoint, params, m_params, ShMtBendSide::LEFT_SIDE ); break; // смещение для длины относительно опорного объекта слева
        case ehp_ShMtBendRightSideObjectOffset: util->UpdateSideObjectOffsetHotPoint( *hotPoint, params, m_params, ShMtBendSide::RIGHT_SIDE ); break; // смещение для длины относительно опорного объекта справа
        case ehp_ShMtBendSideLeftDistance: util->UpdateSideLeftDistanceHotPoint( *hotPoint, params, m_params ); break; // отступ от края сгиба левой стороны
        case ehp_ShMtBendSideRightDistance: util->UpdateSideRightDistanceHotPoint( *hotPoint, params, m_params ); break; // отступ от края сгиба правой стороны 
        case ehp_ShMtBendWidth: util->UpdateWidthDistanceHotPoint( *hotPoint, params, m_params ); break; // ширина
        case ehp_ShMtBendDirection: util->UpdateDirctionHotPoint( *hotPoint, m_params ); break; // направление
        case ehp_ShMtBendSideLeftWidening: util->UpdateSideLeftWideningHotPoint( *hotPoint, params, m_params ); break; // расширение  продолжения (прямой части) сгиба левой стороны
        case ehp_ShMtBendSideRightWidening: util->UpdateSideRightWideningHotPoint( *hotPoint, params, m_params ); break; // расширение  продолжения (прямой части) сгиба правой стороны 
        case ehp_ShMtBendWidthBend: util->UpdateWidthDismissalHotPoint( *hotPoint, params, m_params ); break; // ширина разгрузки сгиба
        case ehp_ShMtBendDepthBend: util->UpdateDepthDismissalHotPoint( *hotPoint, params, m_params ); break; // глубина разгрузки сгиба

        case ehp_ShMtBendAngle: // угол сгиба
        {
          ((HotPointBendAngleParam *)hotPoint)->params = params;
          util->UpdateAngleHotPoint( *hotPoint, params, m_params );
        }
        break;

        case ehp_ShMtBendRadius: // радиус сгиба
        {
          // хот-точку радиуса отображаем только если сгиб выполнен по параметрам самой операции
          if ( bendParam && bendParam->m_byOwner )
          {
            ((HotPointBendRadiusParam *)hotPoint)->params = params;
            util->UpdateRadiusHotPoint( *hotPoint, params, m_params );
          }
          else
            hotPoint->SetBasePoint( nullptr );
        }
        break;

        case ehp_ShMtBendSideLeftAngle: // угол уклона края сгиба левой стороны
        {
          ((HotPointBendParam*)hotPoint)->params = params;
          util->UpdateSideLeftAngleHotPoint( *hotPoint, params, m_params );
        }
        break;

        case ehp_ShMtBendSideRightAngle: // угол уклона края сгиба правой стороны
        {
          ((HotPointBendParam*)hotPoint)->params = params;
          util->UpdateSideRightAngleHotPoint( *hotPoint, params, m_params );
        }
        break;

        case ehp_ShMtBendSideLeftDeviation: // угол уклона продолжения (прямой части) сгиба левой стороны
        {
          ((HotPointBendParam*)hotPoint)->params = params;
          util->UpdateSideLeftDeviationHotPoint( *hotPoint, params, m_params );
        }
        break;

        case ehp_ShMtBendSideRightDeviation: // угол уклона продолжения (прямой части) сгиба правой стороны
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
Начало перетаскивания хот-точки (на хот-точке нажата левая кнопка мыши)
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
Завершение перетаскивания хот-точки (на хот-точке отпущена левая кнопка мыши)
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
// Перерасчет при перетаскивании хот-точки 
/**
\param hotPoint - сдвинутая хот-точка
\param vector - вектор сдвига
\param step - шаг дискретности, для определения смещения с заданным пользователем шагом
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

  case ehp_ShMtBendSideLeftDistance: // отступ от края сгиба левой стороны
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideLeft.m_distance, -5E+5, 5E+5 );
    break;

  case ehp_ShMtBendSideRightDistance: // отступ от края сгиба правой стороны
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideRight.m_distance, -5E+5, 5E+5 );
    break;

  case ehp_ShMtBendWidth: // ширина
    ret = SetHotPointVector( (HotPointVectorParam&)hotPoint, vector, step,
      m_params.m_width, 0.0002, 5E+5 );
    break;

  case ehp_ShMtBendDirection: // направление
    ret = ((HotPointDirectionParam &)hotPoint).SetHotPoint( vector,
      m_params.m_dirAngle,
      pD3DrawTool );
    break;

  case ehp_ShMtBendRadius: // радиус сгиба
    ret = ((HotPointBendRadiusParam &)hotPoint).SetRadiusHotPoint( vector, step,
      m_params, GetThickness() );
    break;

  case ehp_ShMtBendSideLeftWidening: // расширение  продолжения (прямой части) сгиба левой стороны 
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideLeft.m_widening, -5E+5, 5E+5 );
    break;

  case ehp_ShMtBendSideRightWidening: // расширение  продолжения (прямой части) сгиба правой стороны 
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.m_sideRight.m_widening, -5E+5, 5E+5 );
    break;

  case ehp_ShMtBendSideLeftAngle: // угол уклона края сгиба левой стороны 
    ret = SetHotPointVectorAngle( (HotPointBendParam &)hotPoint, vector, step,
      m_params.m_sideLeft.m_angle, -90.0, 90.0, GetThickness() );
    break;

  case ehp_ShMtBendSideRightAngle: // угол уклона края сгиба правой стороны 
    ret = SetHotPointVectorAngle( (HotPointBendParam &)hotPoint, vector, step,
      m_params.m_sideRight.m_angle, -90.0, 90.0, GetThickness() );
    break;

  case ehp_ShMtBendSideLeftDeviation: // угол уклона продолжения (прямой части) сгиба левой стороны
    ret = SetHotPointVectorDeviation( (HotPointBendParam &)hotPoint, vector,
      step, m_params.m_sideLeft.m_deviation,
      -90.0, 90.0, ShMtBendSide::LEFT_SIDE );
    break;

  case ehp_ShMtBendSideRightDeviation: // угол уклона продолжения (прямой части) сгиба правой стороны
    ret = SetHotPointVectorDeviation( (HotPointBendParam &)hotPoint, vector,
      step, m_params.m_sideRight.m_deviation,
      -90.0, 90.0, ShMtBendSide::RIGHT_SIDE );
    break;

  case ehp_ShMtBendWidthBend: // ширина  разгрузки сгиба
    ret = SetHotPointVector( (HotPointVectorParam &)hotPoint, vector, step,
      m_params.GetWidthVar(), 0.00, 5E+5 );
    break;

  case ehp_ShMtBendDepthBend: // глубина  разгрузки сгиба
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
Специальный класс для расчетов
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
      m_params.ReadBaseIfChanged( editComponent ); // если параметры развертки брать из базы, то перечитать базу, если надо
      m_params.GetValues( params, curveEdge->GetMetricLength(), GetObjVersion() );
      if ( GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0800000BL )
        // Смещение посчитать после всех вычислений
        m_params.CalcAfterValues( params );

      if ( children )
      {
        CreatorForBendUnfoldParams _creatorBends( *this, editComponent );
        _creatorBends.Build( index, children->m_extraName );
        _creatorBends.GetParams( params, m_params.m_dirAngle, index - 1 );
      }

      if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
        // Смещение посчитать после всех вычислений
        m_params.CalcAfterValues( params );

      // В некоторых вычислениях необходимо игнорировать состояние разогнутости
      util = new HotPointBendOperUtil( *curveEdge, params, ignoreStraightMode ? false : IsStraighten(), GetThickness() );
    }
  }

  return util;
}


//--------------------------------------------------------------------------------
/**
Создать хот-точки (зарезервированы индексы с 5000 до 5999)
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
    topComponent ) ); // радиус сгиба 
}


//-----------------------------------------------------------------------------
// Перерасчет при перетаскивании хот-точки 
/**
\param hotPoint - перетаскиваемая хот-точка
\param vector - вектор смещения
\param step - шаг дискретности, для определения смещения с заданным пользователем шагом
\param children - сгиб, для которого определяем хот-точки
*/
//---
bool ShMtBend::ChangeChildrenHotPoint( HotPointParam &  hotPoint,
  const MbVector3D &     vector,
  double           step,
  ShMtBendObject & children )
{
  bool ret = false;

  if ( hotPoint.GetIdent() == ehp_ShMtBendObjectRadius )            // радиус сгиба
    ret = ((HotPointBendRadiusParam &)hotPoint).SetRadiusHotPoint( vector, step, children, GetThickness() );

  return ret;
}


//------------------------------------------------------------------------------
/**
Могут ли к объекту быть привязаны управляющие размеры
\return могут ли к объекту быть привязаны управляющие размеры
*/
//---
bool ShMtBend::IsDimensionsAllowed() const
{
  return true;
}


//--------------------------------------------------------------------------------
/**
Получить хот-точки для размеров
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
Ассоциировать размеры с переменными родителя
\param dimensions - размеры
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
Создать размеры объекта по хот-точкам
\param un - именователь
\param dimensions - размеры
\param hotPoints - хот-точки
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
обновить геометрию размеров объекта по хот-точкам
\param dimensions - размеры
\param hotPoints - хот-точки
*/
//---
void ShMtBend::UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  const PArray<OperationSpecificDispositionVariant>* pVariants )
{
  ShMtBaseBend::UpdateDimensionsByHotPoints( dimensions, hotPoints, pVariants );

  if ( hotPoints.Count() == 0 )
    return;

  // #84969 В случае проблем с фантомом удалим размеры
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
    // Есть ребро, установим опорные объекты (их может не быть)
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
Обнулить размер радиуса
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
Обнулить размер угла
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
Ассоциировать размеры дочерних объектов - сгибов
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
Создать размеры объекта по хот-точкам для дочерних объектов
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
обновить геометрию размеров объекта по хот-точкам для дочерних объектов
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
Получить длинну внутренней дуги сгиба
\return длинна внутренней дуги сгиба
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
Обновить рамеры угла уклона сторон сгиба
\param dimensions - размеры
\param utils - параметры хот-точек
\param hotPoints - хот-точки операции
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
Получить параметры размера угла уклона стороны сгиба
\param utils - параметры хот-точек
\param hotPoints - хот-точки операции
\param dimensionLength - длинна выносных линий
\param hotPointId - id хот-точки связанной с размером
\param angle - значение угла уклона
\param dimensionCenter - центральная точка размера размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
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
Обновить размер освобождения - глубина
\param dimensions - размеры операции
\param utils - параметры хот-точек
\param hotPoints - хот-точки операции
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
Получить параметры размера освобождения глубины
\param utils - параметры хот-точек
\param hotPoints - хот-точки операции
\param dimensionPlacement - положение размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
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
Обновить размер освобождения - ширина
\param dimensions - размеры операции
\param utils - параметры хот-точек
\param hotPoints - хот-точки операции
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
Получить параметры размера освобождения ширины
\param pDimensions - размеры операции
\param pUtils - параметры хот-точек
\param pHotPoints - хот-точки операции
\param pDimensionPlacement - положение размера
\param pDimensionPoint1 - точка 1 размера
\param pDimensionPoint2 - точка 2 размера
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
Обновить размеры уклона продолжения сгиба
\param dimensions - размеры операции
\param utils - параметры хот-точек
\param hotPoints - хот-точки операции
*/
//---
void ShMtBend::UpdateDeviationDimensions( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const HotPointBendOperUtil & utils,
  const RPArray<HotPointParam> & hotPoints ) const
{
  // Левая и правая сторона разделяются т.к. для объединения требуется выносить много параметров
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
Получить параметры размера уклона продолжения
\param dimensions - размеры
\param utils - параметры хот-точек
\param hotPoints - хот-точки
\param deviationHotPointId - id хот-точки уклона продолжения
\param sideAngleHotPointId - id хот-точки уклона сгиба
\param dimensionCenter - центральная точка размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
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
Обновить размер расширения сгиба
\param pDimensions - размеры операции
\param pUtils - параметры хот-точек
\param pHotPoints - хот-точки операции
*/
//---
void ShMtBend::UpdateWideningDimensions( const SFDPArray<GenerativeDimensionDescriptor> & pDimensions,
  const HotPointBendOperUtil & pUtils,
  const RPArray<HotPointParam> & pHotPoints )
{
  // левая и правая сторона разделяются т.к. для объединения требуется выносить много параметров
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
Получить параметры размера расширения
\param utils - параметры хот-точек
\param hotPoints - хот-точки
\param wideningHotPointId - id хот-точки размера расширения для которого расчитываются параметры
\param widening - величина расширения
\param dimensionPlacement - положение размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
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
Обновить размер отступа сгиба. Левый или правый, определяется по id хот-точки и размера
\param pDimensions - размеры операции
\param pUtils - параметры хот-точек
\param pHotPoints - хот-точки операции
\param pHotPointId - id хот-точки связанной с размером
\param pDimensionId - id размера
\param pIndent - величина отсупа
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
Получить параметры размера отступа сгиба
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
Обновить размер ширины сгиба
\param pDimensions - размеры операции
\param pUtils - параметры хот-точек
\param pHotPoints - хот-точеки операции
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
Получить параметры размера ширины сгиба
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
Обновить размер смещение сгиба
\param dimensions - размеры операции
\param utils - параметры хот-точек
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
Получить параметры размера смещение сгиба
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
Обновить размер радиуса сгиба ( или дечернего сгиба )
\param dimensions - размеры операции
\param utils - параметры хот-точек
\param dimensionId - id размера радиуса
\param moveToExternal - сместить на внешний радиус
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
Получить параметры размера радиуса сгиба ( или дечернего сгиба )
\return удалось ли получить параметры размера
*/
//--- 
bool ShMtBend::GetRadiusDimensionParameters( const HotPointBendOperUtil & utils, bool moveToExternal,
  MbPlacement3D & dimensionPlacement, MbArc3D & dimensionArc )
{
  if ( (utils.m_pDeepness == nullptr) || (utils.m_pRadius == nullptr) || (utils.m_pAngle == nullptr) )
    return false;

  // важен порядок
  MbArc3D arc( utils.m_pRadius->m_pPoint, utils.m_pAngle->m_pPoint, utils.m_pDeepness->m_pPoint, 1, true );

  // перемещение на внешний радиус
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
Обновить размер длины сгиба
\param dimensions - размеры операции
\param utils - параметры хот-точек
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
Обновить размер длины сгиба
\param pDimensions - размеры операции
\param pUtils - параметры хот-точек
\param isLeftSide - расчет для левой стороны сгиба? Иначе для правой.
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
Получить параметры размера длины сгиба
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
Получить параметры размера длины по касанию
\param utils - параметры хот-точек
\param dimensionPlacement - местоположение размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
\return удалось ли получить параметры размера
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
Получить параметры размера длины по касанию при угле больше 90
\param utils - параметры хот-точек
\param dimensionPlacement - местоположение размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
\return удалось ли получить параметры размера
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
Получить параметры размера длины по контуру
\param pUtils - параметры хот-точек
\param pDimensionPlacement - местоположение размера
\param pDimensionPoint1 - точка 1 размера
\param pDimensionPoint2 - точка 2 размера
\return удалось ли подсчитать параметры размера
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
получить параметры размера длины по продолжению
\param utils - параметры хот-точек
\param dimensionPlacement - местоположение размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
\return удалось ли получить параметры размера
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
Получить параметры размера длины по продолжению слева/справа
\return удалось ли получить параметры размера
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
Получить параметры размера продолжения сгиба слева/справа
\return удалось ли подсчитать параметры размера
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
Расчет параметров размера для боковой длины продолжения сгибю по касанию
\return удалось ли подсчитать параметры размера
*/
//---
bool ShMtBend::GetSideLengthDimensionParametersByTouch( const HotPointBendOperUtil & utils,
  MbPlacement3D       & dimensionPlacement,
  MbCartPoint3D       & dimensionPoint1,
  MbCartPoint3D       & dimensionPoint2,
  bool                  isInternal,
  bool                  isLeftSide ) const
{
  // На самом деле отличий в коде от расчета "по контуру" никаких, поэтому вызываем функцию для него
  // Разница внутри: в pUtils, в расчете конечной точки.
  return GetSideLengthDimensionParametersByContour( utils, dimensionPlacement, dimensionPoint1, dimensionPoint2, isInternal, isLeftSide );
}


//------------------------------------------------------------------------------
/**
Получить касательную к дуге в точке
\param pSupportArc - дуга проходящая через хот-точки
\param pPointOnArc - точка на дуге (ближайшая проекция)
\param pTangent - касательная
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
Обновить размер угла сгиба
\param dimensions - размеры операции
\param utils - параметры хот-точек
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
Получить параметры размеры Угол, тип дополняющий
\param utils - параметры хот-точек
\param dimensionCenter - центр размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
\return удалось ли получить параметры размера
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
Получить параметры размеры Угол, тип - угол сгиба
\param utils - параметры хот-точек
\param dimensionCenter - центр размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
\return удалось ли получить параметры размера
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
Вычислить параметры размера смещения относительно опорного объекта для продолжения сгиба слева/справа
\return удалось ли получить параметры размера
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
Обновить размер смещения относительно опорного объекта для продолжения сгиба слева/справа
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
// Пересчитать местоположение хот-точек
/**
\param hotPoints - хот-точки
\param children - сгиб, для которого определяем хот-точки
*/
//---
void ShMtBend::UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  ShMtBendObject &         children )
{
  if ( hotPoints.Count() )
  {
    bool del = true;

    // Хот-точку радиуса отображаем только если сгиб выполнен по параметрам самого сгиба
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
          if ( hotPoint->GetIdent() == ehp_ShMtBendObjectRadius ) // радиус сгиба
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
// *** Хот-точки ***************************************************************


//--------------------------------------------------------------------------------
/**
Чтение из потока
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
    // опорные объекты для задания длины продолжения сгиба
    in >> obj->m_refObjects;
  }

  in >> obj->m_params;  // параметры операции сгиба

  if ( in.AppVersion() >= 0x1100003FL )
    in >> obj->m_typeCircuitBend;
}


//--------------------------------------------------------------------------------
/**
Запись в поток
\param out
\param obj
\return void
*/
//---
void ShMtBend::Write( writer &out, const ShMtBend * obj )
{
  if ( out.AppVersion() < 0x06000033L )
    out.setState( io::cantWriteObject ); // Не пишем в старые версии
  else
  {
    WriteBase( out, (const ShMtBaseBend *)obj );

    if ( out.AppVersion() >= 0x10000004L )
    {
      // Опорные объекты для задания длины продолжения сгиба
      out << obj->m_refObjects;
    }

    out << obj->m_params;  // параметры операции сгиба

    if ( out.AppVersion() >= 0x1100003FL )
      out << obj->m_typeCircuitBend;
  }
}


//--------------------------------------------------------------------------------
/**
Действия после чтения
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
Необходимо ли сохранение без истории
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
        return false; // уже сохранили с конвертированием
    }

    // Сюда мы попали, когда не смогли конвертировать в старую версию.
    // Сохраняем без истории.
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
Установить опорный объект
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
Получить опорный объект
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
Рассчитать расстояние до опорного объекта для стороны сгиба с учетом смещения
\param utils
\param isLeftSide
\param length
\return const bool
*/
//---
const bool ShMtBend::CalculateSideLengthToObject( HotPointBendOperUtil & utils, bool isLeftSide, double & length )
{
  MbCartPoint3D endPoint; // Конечная точка сгиба
  MbVector3D    axisVec;  // Осевой вектор
  MbCartPoint3D cp;
  if ( utils.CalculateSideObjectOffsetParams( cp, endPoint, axisVec, m_params, isLeftSide ) )
  {
    MbCartPoint3D originPoint;
    utils.GetSideLengthOriginPoint( originPoint, m_params.m_bendLengthBy2Sides, isLeftSide );

    // Построение разделяющей плоскости
    MbPlacement3D _place( originPoint, axisVec );
    MbPlane dividingPlane( _place );

    MbeItemLocation placementRes = dividingPlane.PointRelative( endPoint, ANGLE_EPSILON );
    double movedVertexDist = originPoint.DistanceToPoint( endPoint );

    // Подходит только если точка находится "над" или "в" плоскости сгиба
    bool positiveDist = (placementRes == MbeItemLocation::iloc_InItem || placementRes == MbeItemLocation::iloc_OnItem);
    if ( !positiveDist )
      movedVertexDist = -movedVertexDist;
    length = movedVertexDist;
    return length >= 0;
  }

  // Ставим отрицательную длину: расчет не удался
  length = -1;
  return false;
}


//------------------------------------------------------------------------------
/**
Конвертировать для сохранения в версию 5.11
*/
//--- 
void ShMtBend::ConvertTo5_11( IfCanConvert & owner, const WritingParms & wrParms )
{
  // Сюда не попадаем: даже базовый класс не сохраняется в 5.11
}


//------------------------------------------------------------------------------
/**
Конвертировать для сохранения в предыдующую версию
*/
//--- 
void ShMtBend::ConvertToSavePrevious( SaveDocumentVersion saveMode, IfCanConvert & owner, const WritingParms & wrParms )
{
  if ( m_params.m_bendLengthBy2Sides )
  {
    // Предполагаем, что обе длины заданы абсолютно и параметрически равны.
    // В противном случае данная конвертация будет ошибочна.
    m_params.m_bendLengthBy2Sides = false;
    // Длина правильная, место отсчета длины можно оставить.
  }
  else
  {
    if ( m_params.m_sideLeft.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute )
    {
      m_params.m_sideLeft.m_lengthCalcMethod = ShMtBendSide::LengthCalcMethod::lcm_absolute;
      // Важно! Ставим тип задания длины от конца дуги места сгиба (умолчательный)
      // В противном случае при перестроении продолжение может измениться по длине.
      m_params.m_sideLeft.m_absLengthType = ShMtBendSide::alt_byContinue;
    }
  }
  // KOMPAS-23638   BEGIN
  if ( saveMode < SaveDocumentVersion::sdv_Kompas_18
    && m_params.m_typeRemoval == ShMtBaseBendParameters::tr_byCentre )
  {                                     // изменить способ размещения "По линии сгиба" на "Смещение внутрь"
    m_params.m_typeRemoval = ShMtBaseBendParameters::tr_byIn;

    MbBendValues mbParams;              // получить общий для всех подсгибов радиус
    ((ShMtBaseBendParameters)m_params).GetValues( mbParams );

    IFC_Array<ShMtBendObject> children; // получить подсгибы
    GetChildrens( children );
    if ( children.Count() )             // KOMPAS-23920: подменить общий радиус радиусом первого подсгиба
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
Следует ли конвертировать для сохранения в версию 5.11
*/
//--- 
bool ShMtBend::IsNeedConvertTo5_11( SSArray<uint> & types, IfCanConvert & owner )
{
  // В 5.11 вообще сгибы не сохраняются
  return false;
}


//------------------------------------------------------------------------------
/**
Следует ли конвертировать для сохранения в предыдующую версию
*/
//--- 
bool ShMtBend::IsNeedConvertToSavePrevious( SSArray<uint> & types, SaveDocumentVersion saveMode, IfCanConvert & owner )
{
  bool shouldConvert = false; // Можно (и нужно) ли конвертировать в старую версию?
  if ( saveMode <= SaveDocumentVersion::sdv_Kompas_15_Sp2 )
  {
    // Нам нужно конвертировать, но можем ли мы?
    // Конвертировать в старую версию можно будет при условии, что у нас равнобокий сгиб (не по 2-м сторонам),
    // либо обе стороны эквивалентны, при этом длина задается в абсолютной форме.
    // Так же здесь важным моментом является то, что способ отсчета длины должен совпадать.
    if ( m_params.m_bendLengthBy2Sides &&
      m_params.m_sideLeft.m_lengthCalcMethod == ShMtBendSide::LengthCalcMethod::lcm_absolute &&
      m_params.m_sideRight.m_lengthCalcMethod == ShMtBendSide::LengthCalcMethod::lcm_absolute &&
      m_params.m_sideLeft.m_absLengthType == m_params.m_sideRight.m_absLengthType )
    {
      // Конвертируем только если обе длины задаются абсолютно и они параметрически равны:
      double len1 = 0.0;
      m_params.m_sideLeft.m_length.GetValue( len1 );
      double len2 = 0.0;
      m_params.m_sideRight.m_length.GetValue( len2 );
      if ( ::abs( len1 - len2 ) <= Math::paramDeltaMin )
        shouldConvert = true;
    }
    else if ( !m_params.m_bendLengthBy2Sides && m_params.m_sideLeft.m_lengthCalcMethod != ShMtBendSide::LengthCalcMethod::lcm_absolute ) // Равнобокий сгиб
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
Посчитать длины сторон продолжения сгиба, если они заданы не в абсолютном виде
\param params
\return bool
*/
//---
bool ShMtBend::UpdateSideLengthsForNonAbsoluteCalcMode( MbBendByEdgeValues & params )
{
  bool suitableParamValues = true; // Сгиб может испортиться в случае каких-то некорректных значений, например,
                                   // когда вершина (точка на проекционной плоскости) будет в противоположной стороне от направления сгиба.

  std::auto_ptr<HotPointBendOperUtil> utils( GetHotPointUtil( params,
    m_bendObject.GetObjectTrans( 0 ).editComponent,
    m_bendObjects.Count() ? m_bendObjects[0] : nullptr,
    true ) );
  // VB Здесь utils можно не проверять на null. Выше всегда проверяется базовое ребро, от которого они зависят.
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
Есть ли зависимость от присланного объекта?
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
Проверить, выбраны ли опорные объекты для неабсолютных длин сторон сгиба
\return bool Все ли в порядке с опорными объектами
*/
//---
bool ShMtBend::IsValidBaseObjects()
{
  bool res = true;

  // Проверяем левую сторону
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

  // Проверяем правую сторону
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
Получить местоположение объекта относительно плоскости сгиба.
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

    // Построение разделяющей плоскости
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

    // Обязательная инициализация
    double projX( UNDEFINED_DBL ), projY( UNDEFINED_DBL );
    if ( planePlacement.DirectPointProjection( originPoint, directionVec, projX, projY ) )
    {
      // Спроецировали, но вот вопрос: с нужной ли стороны от начала сгиба получилась проекция?
      MbCartPoint3D projectedPoint;
      planePlacement.PointOn( projX, projY, projectedPoint );

      // Построение разделяющей плоскости
      MbPlacement3D _place( originPoint, directionVec );
      MbPlane dividingPlane( _place );

      result = dividingPlane.PointRelative( projectedPoint, ANGLE_EPSILON );
    }
  }

  return result;
}


//--------------------------------------------------------------------------------
/**
Нужно ли инвертировать направления сгиба из-за пололжения опорного объекта, до которого строим сгиб.
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
Можно ли использовать данный объект как опорную вершину?
\param pointWrapper
\return const bool
*/
//---
const bool ShMtBend::IsObjectSuitableAsBaseVertex( const WrapSomething & pointWrapper )
{
  MbeItemLocation location = IFPTR( AnyPoint )(pointWrapper.obj) ? BendObjectLocation( pointWrapper, true/*Тут все равно*/ ) : MbeItemLocation::iloc_Undefined;

  return location == MbeItemLocation::iloc_InItem || location == MbeItemLocation::iloc_OutOfItem;
}


//--------------------------------------------------------------------------------
/**
Можно ли использовать данный объект как опорную плоскость?
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
Исправить параметр длины стороны продолжения сгиба
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
Получить все контейнеры ссылок у операции
\param refContainers Контейнеры ссылок операции
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
// Класс операции сгиб по линии
//
////////////////////////////////////////////////////////////////////////////////
class ShMtBaseBendLine : public IfShMtBendLine,
  public ShMtBaseBend,
  public IfHotPoints
{
public:
  ReferenceContainer      m_bendFaces;      // грани которые гнем

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
  virtual void                              ResetAllBendFace() override; // удалить все face который гнем
  virtual size_t                            GetBendFaceCount() const override; // к-во faces который гнем
  virtual WrapSomething *                   GetBendFace( uint i ) override; // i - face который гнем
  virtual const WrapSomething *             GetBendFace( uint i ) const override;
  virtual SArray<WrapSomething>             GetBaseFaces() const override;
#ifdef MANY_THICKNESS_BODY_SHMT
  virtual bool                              IsSuitedObject( const WrapSomething & ) override;
#else
  virtual bool                              IsSuitedObject( const WrapSomething &, double ) override;
#endif  
  virtual bool                              IsLeftSideFix() = 0;
  //--------------- общие функции объекта детали -------------------------------//
  virtual void            Assign( const PrObject &other ) override;           // установить параметры по другому объекту

  virtual bool            RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath ) override; // восстановить связи
  virtual void            ClearReferencedObjects( bool soft ) override;
  virtual bool            ChangeReferences( ChangeReferencesData & changedRefs ) override; ///< Заменить ссылки на объекты
  virtual bool            IsValid() const override;  // проверка определенности объекта
  virtual void            WhatsWrong( WarningsArray & ); // набирает массив предупреждений из ресурса
  virtual bool            IsThisParent( uint8 relType, const WrapSomething & ) const override;
  virtual void            GetVariableParameters( VarParArray &, ParCollType parCollType ) override;
  virtual void            ForgetVariables() override; // ИР K6+ забыть все переменные, связанные с параметрами
  virtual bool            PrepareToAdding( PartRebuildResult &, PartObject * ) override; //подготовиться ко встраиванию в модель
                                                                                         // создание фантомного или результативного Solida
                                                                                         // код ошибки необходимо обязательно проинициализировать
  virtual MbSolid       * MakeSolid( uint              & codeError,
    MakeSolidParam    & mksdParam,
    uint                shellThreadId ) override;
  // приготовить свое, характерное тело
  virtual uint            PrepareInterimSolid( MbCurve3D            & curve,
    RPArray<MbFace>      & mbFaces,
    MbSNameMaker         & name,
    MbSolid              & prevSolid,
    MbBendValues         & params,        // свои уже приготовленные параметры
    const IfComponent    * /*operOwner*/, // компонент - владелец этой операции
    const IfComponent    * /*bodyOwner*/, // компонент - владелец тела, над которым эта операция производиться
    uint                   idShellThread,
    MbSolid              *&currSolid,
    WhatIsDone            wDone );
  // приготовить свое, характерное тело
  virtual uint            PrepareLocalSolid( MbCurve3D            & curve,
    RPArray<MbFace>      & mbFaces,
    MbSNameMaker         & name,
    MbSolid              & prevSolid,
    MbBendValues         & params,        // свои уже приготовленные параметры
    const IfComponent    * /*operOwner*/, // компонент - владелец этой операции
    const IfComponent    * /*bodyOwner*/, // компонент - владелец тела, над которым эта операция производиться
    WhatIsDone             wDone,
    uint                   idShellThread,
    MbSolid              *&currSolid ) = 0;
  // приготовить свои параметры
  virtual MbBendValues &  PrepareLocalParameters( const IfComponent * component ) = 0;

  // Реализация IfHotPoints
  /// Создать хот-точки
  virtual void GetHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool ) override;
  /// Начало перетаскивания хот-точки (на хот-точке нажата левая кнопка мыши)
  virtual bool BeginDragHotPoint( HotPointParam & hotPoint ) override;
  /// Перерасчет при перетаскивании хот-точки 
  // step - шаг дискретности, для определения смещения с заданным пользователем шагом
  virtual bool ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
    double step, D3Draw & pD3DrawTool ) override;
  /// Завершение перетаскивания хот-точки (на хот-точке отпущена левая кнопка мыши)
  virtual bool EndDragHotPoint( HotPointParam & hotPoint, D3Draw & pD3DrawTool ) override;

  // Реализация хот-точек радиусов сгибов от ShMtStraighten
  /// Создать хот-точки (зарезервированы индексы с 5000 до 5999)
  virtual void GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ShMtBendObject &         children ) override;
  /// Перерасчет при перетаскивании хот-точки
  virtual bool ChangeChildrenHotPoint( HotPointParam &  hotPoint,
    const MbVector3D &     vector,
    double           step,
    ShMtBendObject & children ) override;

protected:
  virtual const AbsReferenceContainer * GetRefContainer( const IfComponent & bodyOwner ) const override { return &m_bendFaces; }

  /// Пересчитать местоположение хот-точек
  virtual void            UpdateHotPoints( HotPointBaseBendLineOperUtil & util,
    RPArray<HotPointParam> &       hotPoints,
    MbBendOverSegValues &          params,
    D3Draw &                       pD3DrawTool );
  /// Создать размеры объекта по хот-точкам
  virtual void CreateDimensionsByHotPoints( IfUniqueName           * un,
    SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints ) override;
  /// Обновить геометрию размеров объекта по хот-точкам
  void UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util );
  /// Ассоциировать постоянные размеры с переменными родителя
  virtual void AssignDimensionsVariables( const SFDPArray<PartObject>  & dimensions ) override;

  void            CollectFaces( PArray<MbFace> & faces,
    const IfComponent & bodyOwner,
    uint                shellThreadId );
  void            SimplyCollectFaces( PArray<MbFace> & faces );


  /// Обновить хот-точку направления
  void            UpdateDirectionHotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util );
  /// Обновить хот-точку неподвижной стороны
  void            UpdateDirection2HotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util );
  ///Обновить хот-точку радиуса
  void            UpdateRadiusHotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util,
    MbBendOverSegValues & bendParameters );
  /// Обновить хот-точку угла
  void            UpdateAngleHotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBaseBendLineOperUtil & util,
    MbBendOverSegValues & bendParameters );

  bool            IsSavedUnhistoried( long version, ShMtBaseBendParameters::TypeRemoval typeRemoval ) const;

private:
  ShMtBaseBendLine& operator = ( const ShMtBaseBendLine & ); // не реализовано
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
// конструктор дублирования
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
// установить параметры по другому объекту
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
// добавить или удалить face который гнем
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
    res = true;                      // добавили обьект
  }

  return res;
}


//---------------------------------------------------------------------------------------------
// удалить все face который гнем
//---
void ShMtBaseBendLine::ResetAllBendFace()
{
  m_bendFaces.Reset();
  m_bendObjects.Flush();
}


//------------------------------------------------------------------------------
// к-во faces который гнем
// ---
size_t ShMtBaseBendLine::GetBendFaceCount() const
{
  return m_bendFaces.Count();
}


//-------------------------------------------------------------------------
// i - face который гнем
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
// годится ли пришедший объект
// ---
bool ShMtBaseBendLine::IsSuitedObject( const WrapSomething & wrapper )
#else
bool ShMtBaseBendLine::IsSuitedObject( const WrapSomething & wrapper, double thickness )
#endif
{
  bool res = false;
  // ША K11 17.12.2008 Ошибка №37142: Запрещяем выбор ребер многочастных тел
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
          // подходящий грань только в том случае если она принадлежит тому же телу что и выбранная уже
          // или если еще нет грани
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
// Набрать все годные faces
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
// Набрать все годные faces
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
// создание фантомного или результативного Solida
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

        // приготовить свои параметры
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
                mksdParam.m_operOwner, // компонент - владелец этой операции
                mksdParam.m_bodyOwner, // компонент - владелец тела, над которым эта операция производиться
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
// приготовить свое, промежуточное, характерное тело 
//---
uint ShMtBaseBendLine::PrepareInterimSolid( MbCurve3D            & curve,
  RPArray<MbFace>      & mbFaces,
  MbSNameMaker         & nameMaker,
  MbSolid              & prevSolid,
  MbBendValues         & params,    // свои уже приготовленные параметры
  const IfComponent    * operOwner, // компонент - владелец этой операции
  const IfComponent    * bodyOwner, // компонент - владелец тела, над которым эта операция производиться
  uint                   idShellThread,
  MbSolid              *&resultSolid,
  WhatIsDone            wDone )
{
  uint codeError = operOwner ? rt_Success : rt_ParameterError;

  if ( operOwner && mbFaces.Count() ) {
    MbPlacement3D place;
    mbFaces[0]->GetPlacement( &place );
    {
      UtilFindFixedPairFaces * utlFindFixedFaces = wDone == wd_Phantom ? nullptr : new UtilFindFixedPairFaces( mbFaces );  // поиск и сохранение фиксированной грани имени
      codeError = PrepareLocalSolid( curve,
        mbFaces,
        nameMaker,
        prevSolid,
        params,
        operOwner, // компонент - владелец этой операции
        bodyOwner, // компонент - владелец тела, над которым эта операция производиться
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
      // ША K9 19.1.2007 Ошибка №20801 : Теперь разгибается в математике
      // CC K12 Исправление этой ошибки через математику привело к перескакиванию имен граней. Что отобрежено в 
      // CC K12 комменте #8 этой же ошибки. 
      // СС K12 Возвращаю все назад и завожу под версию. В промежутке между этими версиями пусть работает также не коректно.
      // СС K12 +Исправление ошибки 40270
      if ( IsStraighten() && resultSolid )
      {
        codeError = rt_SolidError;
        MbBendValues paramBends;           // параметры листового тела
                                           // СС K12 			 paramBends.thickness = params.thickness;

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
                MbFace * outerFace = ::FindOppositeFace( *innerFace, GetThickness()/* CC К12 params.thickness*/, st_CylinderSurface );
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
          // поиск фиксированной грани
          MbFace *faceFix = nullptr;
          MbPlacement3D pl;
          // 1. на совпадени плайсов граней
          for ( size_t i = 0, c = faces.Count(); i < c; i++ )
          {
            if ( faces[i]->GetPlacement( &pl ) && pl.Complanar( place ) )
            {
              faceFix = faces[i];
              break;
            }
          }

          // 2. боковая грань является фиксом 
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

          // 3. любая параллельная грань является фиксом
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
            codeError = UnbendSheetSolid( *resultSolid,         // исходное тело
              cm_KeepHistory,  // НГ работать с копией оболочки исходного тела
              bend,            // массив сгибов, состоящих из пар граней - внутренней и внешней граней сгиба
              *faceFix,        // грань, остающаяся неподвижной
              fixedPoint,      // точка в параметрической области faceFix, в которой разогнутая грань касается согнутой
                               // CC K12 																				params,          // параметры листового тела
              nameMaker,            // именователь
              resultSolid );        // результирующее тело
            if ( resultSolid != oldResult )
              ::DeleteItem( oldResult );
          }
        }
      }
    }
  }

  return codeError;
}
// НМА K7+ #endif


//------------------------------------------------------------------------------
// проверка определенности операции
// ---
bool ShMtBaseBendLine::IsValid() const
{
  bool res = ShMtBaseBend::IsValid();
  if ( res )
    res = m_bendFaces.IsFull();

  return res;
}


//------------------------------------------------------------------------------
// восстановить связи
// ---
bool ShMtBaseBendLine::RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath )
{
  bool res = ShMtBaseBend::RestoreReferences( result, allRefs, withMath );
  bool resObj = m_bendFaces.RestoreReferences( result, allRefs, withMath );

  if ( IsJustRead() && GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0B000001L )
  {
    // для версий старше 11 надо проставить ид нитки в подсгибы
    IFPTR( Primitive ) prim( m_bendFaces.GetObjectTrans( 0 ).obj );
    if ( prim )
      for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
        m_bendObjects[i]->m_extraName = prim->GetShellThreadId();
  }

  return res && resObj;
} // RestoreReferences


  //------------------------------------------------------------------------------
  // Очистить опорные объекты soft = true - только враперы (не трогая имена)
  // ---
void ShMtBaseBendLine::ClearReferencedObjects( bool soft )
{
  ShMtBaseBend::ClearReferencedObjects( soft );
  m_bendFaces.ClearObjects( soft );
}


//------------------------------------------------------------------------------
// Заменить ссылки на объекты
// ---
bool ShMtBaseBendLine::ChangeReferences( ChangeReferencesData & changedRefs )
{
  bool res = ShMtBaseBend::ChangeReferences( changedRefs );
  if ( m_bendFaces.ChangeReferences( changedRefs ) )
    res = true;

  return res;
}


//------------------------------------------------------------------------------
// набирает массив предупреждений из ресурса
// ---
void ShMtBaseBendLine::WhatsWrong( WarningsArray & warnings ) {
  ShMtBaseBend::WhatsWrong( warnings );

  for ( size_t i = warnings.Count(); i--; )
  {
    if ( warnings[i]->GetResId() == IDP_RT_NOINTERSECT )// KA V17 16.05.2016 KOMPAS-7873 При не пересечении, добавим еще одно собщение
      warnings.AddWarning( IDP_SF_ERROR, module );
  }

  if ( !m_bendFaces.IsFull() )
    warnings.AddWarning( IDP_WARNING_LOSE_SUPPORT_OBJ, module ); // "Потерян один или несколько опорных объектов"
}


//------------------------------------------------------------------------------
// задать кривую сгиба
// ---
void ShMtBaseBendLine::SetBendObject( const WrapSomething * wrapper ) {
  ShMtBaseBend::SetObject( wrapper );
}


//------------------------------------------------------------------------------
// получить кривую сгиба
// ---
const WrapSomething & ShMtBaseBendLine::GetBendObject() const {
  return ShMtBaseBend::GetObject();
}


//-----------------------------------------------------------------------------
//подготовиться ко встраиванию в модель
//---
bool ShMtBaseBendLine::PrepareToAdding( PartRebuildResult & result, PartObject * toObj )
{
  if ( ShMtBaseBend::PrepareToAdding( result, toObj ) )
  {
    if ( !GetFlagValue( O3D_IsReading ) ) // не прочитан из памяти, а создан как new  BUG 68896
    {
      GetParameters().PrepareToAdding( result, this );
      UpdateMeasurePars( gtt_none, true );
    }
    return true;
  }

  return false;
}


//------------------------------------------------------------------------------
// выдать массив параметров, связанных с переменными
// ---
void ShMtBaseBendLine::GetVariableParameters( VarParArray & parVars, ParCollType parCollType )
{
  ::ShMtSetAsInformation( GetParameters().m_radius, ShMtManager::sm_radius, m_bendObjects.Count() > 0 && !IsAnySubBendByOwner() );

  ShMtBaseBend::GetVariableParameters( parVars, parCollType );
  GetParameters().GetVariableParameters( parVars, parCollType == all_pars, true );
}



//-----------------------------------------------------------------------------
// ИР K6+ забыть все переменные, связанные с параметрами
//---
void ShMtBaseBendLine::ForgetVariables() {
  ShMtBaseBend::ForgetVariables();
  GetParameters().ForgetVariables();
}


// *** Хот-точки ***************************************************************
//------------------------------------------------------------------------------
// Создать хот-точки
// ---
void ShMtBaseBendLine::GetHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  hotPoints.Add( new HotPointDirectionParam( ehp_ShMtBendDirection, editComponent, topComponent ) ); // направление
  hotPoints.Add( new HotPointDirectionParam( ehp_ShMtBendDirection2, editComponent, topComponent ) ); // Неподвижная сторона
  hotPoints.Add( new HotPointBendLineRadiusParam( ehp_ShMtBendRadius, editComponent, topComponent, HotPointVectorParam::ht_move ) ); // радиус сгиба 
}


//-------------------------------------------------------------------------------
// Начало перетаскивания хот-точки (на хот-точке нажата левая кнопка мыши)
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
// Завершение перетаскивания хот-точки (на хот-точке отпущена левая кнопка мыши)
//---
bool ShMtBaseBendLine::EndDragHotPoint( HotPointParam & hotPoint, D3Draw & pD3DrawTool )
{
  // CC K12  return hotPoint.GetIdent() == ehp_ShMtBendAngle;
  return false;
}


//-----------------------------------------------------------------------------
// Перерасчет при перетаскивании хот-точки 
/**
\param hotPoint - сдвинутая хот-точка
\param vector - вектор сдвига
\param step - шаг дискретности, для определения смещения с заданным пользователем шагом
*/
//---
bool ShMtBaseBendLine::ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
  double step, D3Draw & pD3DrawTool )
{
  bool ret = false;

  switch ( hotPoint.GetIdent() )
  {
  case ehp_ShMtBendDirection: // направление
    ret = ((HotPointDirectionParam &)hotPoint).SetHotPoint( vector, GetParameters().m_dirAngle, pD3DrawTool );
    break;

  case ehp_ShMtBendDirection2: // Неподвижная сторона
    ret = ((HotPointDirectionParam &)hotPoint).SetHotPoint( vector,
      ((ShMtBendLineParameters &)GetParameters()).m_leftFixed,
      pD3DrawTool );
    break;

  case ehp_ShMtBendRadius: // радиус сгиба
    ret = ((HotPointBendLineRadiusParam &)hotPoint).SetRadiusHotPoint( vector, step,
      GetParameters(),
      GetParameters(),
      GetThickness() );
    break;

  case ehp_ShMtBendAngle: // угол сгиба
    ret = ::SetHotPointAnglePar( (HotPointBendAngleParam&)hotPoint, vector,
      step, GetParameters() );
    break;
  }

  return ret;
}


//------------------------------------------------------------------------------
/**
Обновить хот-точку направления
\param hotPoints - массив хот-точек операции
\param util - утилита для расчета хот-точек
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
Обновить хот-точку неподвижной стороны
\param hotPoints - массив хот-точек
\param util - утилита для расчета хот-точек
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
Обновить хот-точку радиуса
\param hotPoints - массив хот-точек
\param util - утилита для расчета хот-точек
\param bendParameters - параметры сгиба
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
Обновить хот-точку угла
\param hotPoints - массив хот-точек
\param util - утилита для расчета хот-точек
\param bendParameters - параметры сгиба
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
// Пересчитать местоположение хот-точек
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
Создать размеры объекта по хот-точкам
\param  un         - унивальное имя
\param  dimensions - размеры
\param  hotPoints  - хот-точки
\return нет
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
Обновить геометрию размеров объекта по хот-точкам
\param  dimensions - размеры
\param  hotPoints  - хот-точки
\return нет
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

    //Угловой размер
    IFPTR( OperationAngularDimension ) anglDim( ::FindGnrDimension( dimensions, ehp_ShMtBendAngle, false ) );
    if ( anglDim )
      util.UpdateAngularBendDim( *anglDim, *pars );
  }
}


//------------------------------------------------------------------------------
/**
Ассоциировать постоянные размеры с переменными родителя
\param  dimensions - размеры
\return нет
*/
//---
void ShMtBaseBendLine::AssignDimensionsVariables( const SFDPArray<PartObject> & dimensions )
{
  if ( ShMtBendLineParameters * pars = dynamic_cast<ShMtBendLineParameters*>(&GetParameters()) )
    ::UpdateDimensionVar( dimensions, ehp_ShMtBendRadius, pars->m_radius );
}


//------------------------------------------------------------------------------
// Создать хот-точки (зарезервированы индексы с 5000 до 5999)
// ---
void ShMtBaseBendLine::GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ShMtBendObject &         children )
{
  hotPoints.Add( new HotPointBendLineRadiusParam( ehp_ShMtBendObjectRadius, editComponent,
    topComponent ) ); // радиус сгиба 
}


//-----------------------------------------------------------------------------
// Перерасчет при перетаскивании хот-точки 
/**
\param hotPoint - перетаскиваемая хот-точка
\param vector - вектор смещения
\param step - шаг дискретности, для определения смещения с заданным пользователем шагом
\param children - сгиб, для которого определяем хот-точки
*/
//---
bool ShMtBaseBendLine::ChangeChildrenHotPoint( HotPointParam &  hotPoint,
  const MbVector3D &     vector,
  double           step,
  ShMtBendObject & children )
{
  bool ret = false;

  if ( hotPoint.GetIdent() == ehp_ShMtBendObjectRadius ) // радиус сгиба
    ret = ((HotPointBendLineRadiusParam &)hotPoint).SetRadiusHotPoint( vector, step, GetParameters(), children, GetThickness() );

  return ret;
}
// *** Хот-точки ***************************************************************


//--------------------------------------------------------------------------------
/**
Необходимо ли сохранение без истории
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
// Чтение из потока
// ---
void ShMtBaseBendLine::Read( reader &in, ShMtBaseBendLine * obj ) {
  ReadBase( in, (ShMtBaseBend *)obj );
  in >> obj->m_bendFaces;
}


//------------------------------------------------------------------------------
// Запись в поток
// ---
void ShMtBaseBendLine::Write( writer &out, const ShMtBaseBendLine * obj ) {
  if ( out.AppVersion() < 0x06000033L )
    out.setState( io::cantWriteObject ); // специально введенный тип ошибочной ситуации
  else {
    WriteBase( out, (const ShMtBaseBend *)obj );
    out << obj->m_bendFaces;
  }
}


////////////////////////////////////////////////////////////////////////////////
//
// класса операций сгиб по линии
//
////////////////////////////////////////////////////////////////////////////////
class ShMtBendLine : public ShMtBaseBendLine,
  public IfCopibleObject,
  public IfCopibleOperation
{
public:
  ShMtBendLineParameters  m_params;

public:
  // конструктор, просто запоминает параметры ничего не рассчитывая, эскиз дадим потом
  // конструктор для использования в конструкторе процесса
  ShMtBendLine( const ShMtBendLineParameters &, IfUniqueName * un );
  // конструктор дублирования
  ShMtBendLine( const ShMtBendLine & );

public:
  I_DECLARE_SOMETHING_FUNCS;

  virtual double                            GetOwnerAbsAngle() const;
  virtual double                            GetOwnerValueBend() const;
  virtual double                            GetOwnerAbsRadius() const;  // внутренний радиус сгиба
  virtual bool                              IsOwnerInternalRad() const;  // по внутреннему радиусу?
  virtual UnfoldType                        GetOwnerTypeUnfold() const;  // дать тип развертки  
  virtual void                              SetOwnerTypeUnfold( UnfoldType t );  // задать тип развертки  
  virtual bool                              GetOwnerFlagByBase() const;  // 
  virtual void                              SetOwnerFlagByBase( bool val );        // 
  virtual double                            GetOwnerCoefficient() const;  // коэффициент
  virtual bool                              IfOwnerBaseChanged( IfComponent * trans );        // если изменились какие либо параметры, и есть чтение коэф из базы, то прочитать базу 
  virtual double                            GetThickness() const;  // дать толщину
  virtual void                              SetThickness( double );// задать толщину      
                                                                   // перед тем как заканчиваем редактирование
  virtual void                              EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp );

  virtual void                              SetParameters( const ShMtBaseBendParameters & );
  virtual ShMtBaseBendParameters          & GetParameters() const;

  virtual bool                              IsLeftSideFix();
  //---------------------------------------------------------------------------
  // IfCopibleObject
  // Создать копию объекта
  virtual IfPartObjectCopy * CreateObjectCopy( IfComponent & compOwner ) const;
  // Следует копировать на указанном компоненте
  virtual bool               NeedCopyOnComp( const IfComponent & component ) const;
  virtual bool               IsAuxGeom() const { return false; } ///< Это вспомогательная геометрия?
                                                                 /// Может ли операция менятся по параметрам в массиве
  virtual bool               CanCopyByVarsTable() const { return false; }

  //---------------------------------------------------------------------------
  // IfCopibleOperation
  // Можно ли копировать эту операцию как-нибудь иначе, кроме как геометрически?
  // Т.е. через MakePureSolid или через интерфейсы IfCopibleOperationOnPlaces,
  // IfCopibleOperationOnEdges и т.д.
  virtual bool            CanCopyNonGeometrically() const;

  //------------------------------------------------------------------------------
  // общие функции объекта детали 
  virtual uint            GetObjType() const;                            // вернуть тип объекта
  virtual uint            GetObjClass() const;                            // вернуть общий тип объекта
  virtual uint            OperationSubType() const; // вернуть подтип операции
  virtual PrObject      & Duplicate() const;

  virtual void            PrepareToBuild( const IfComponent & ); ///< подготовиться к построению
  virtual void            PostBuild(); ///< некие действия после построения
                                       /// приготовить свое, характерное тело
  virtual uint            PrepareLocalSolid( MbCurve3D            & curve,
    RPArray<MbFace>      & mbFaces,
    MbSNameMaker         & nameMaker,
    MbSolid              & prevSolid,
    MbBendValues         & params,        // свои уже приготовленные параметры
    const IfComponent    * /*operOwner*/, // компонент - владелец этой операции
    const IfComponent    * /*bodyOwner*/, // компонент - владелец тела, над которым эта операция производиться
    WhatIsDone             wDone,
    uint                   idShellThread,
    MbSolid              *&currSolid );
  // приготовить свои параметры
  virtual MbBendValues &  PrepareLocalParameters( const IfComponent * component );

  // набирает массив предупреждений из ресурса
  virtual void            WhatsWrong( WarningsArray & warnings );
  virtual bool            IsValid() const;  // проверка определенности объекта

                                            /// Возможно ли для параметра задавать допуск
  virtual bool            IsCanSetTolerance( const VarParameter & varParameter ) const;
  /// Для параметра получить неуказанный предельный допуск
  virtual double          GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const;
  /// Является ли параметр угловым
  virtual bool            ParameterIsAngular( const VarParameter & varParameter ) const;
  virtual bool            CopyInnerProps( const IfPartObject& source,
    uint sourceSubObjectIndex,
    uint destSubObjectIndex ) override;
  /// Надо ли сохранять как объект без истории
  virtual bool            IsNeedSaveAsUnhistored( long version ) const override;

  // Реализация IfHotPoints
  /// Пересчитать местоположение хот-точки
  virtual void UpdateHotPoints( RPArray<HotPointParam> & hotPoints,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool );
  /// Создать хот-точки               
  virtual void GetHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool );
  /// Пересчитать местоположение хот-точек
  virtual void UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    ShMtBendObject &         children ) override;
  /// Могут ли к объекту быть привязаны управляющие размеры
  virtual bool IsDimensionsAllowed() const;
  /// Обновить геометрию размеров объекта по хот-точкам
  virtual void UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    const PArray<OperationSpecificDispositionVariant>* pVariants );
  /// Ассоциировать постоянные размеры с переменными родителя
  virtual void AssignDimensionsVariables( const SFDPArray<PartObject>  & dimensions );
  /// создать размеры объекта по хот-точкам для дочерних объектов
  virtual void CreateChildrenDimensionsByHotPoints( IfUniqueName           * un,
    SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children );
  /// обновить геометрию размеров объекта по хот-точкам для дочерних объектов
  virtual void UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor>  & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject         & children );
  /// Ассоциировать размеры дочерних объектов - сгибов
  virtual void AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
    ShMtBendObject        & children );

  /// изменить уникальное имя параметра
  virtual void              RenameUnique( IfUniqueName & un );

  /// Расчет дуги для пересчета угловых параметров по допуску
  virtual void UpdateMeasurePars( GeneralToleranceType gType, bool recalc );

private:
  std::unique_ptr<HotPointBendLineOperUtil> GetHotPointUtil( MbBendOverSegValues *& params,
    IfComponent & editComponent,
    ShMtBendObject * children );

private:
  void operator = ( const ShMtBendLine & ); // не реализовано

                                            //  DECLARE_PERSISTENT_CLASS( ShMtBendLine )
  DECLARE_PERSISTENT_CLASS( ShMtBendLine )
};



//------------------------------------------------------------------------------
// конструктор, просто запоминает параметры ничего не рассчитывая, эскиз дадим потом
// конструктор для использования в конструкторе процесса
ShMtBendLine::ShMtBendLine( const ShMtBendLineParameters & _pars, IfUniqueName * un )
  : ShMtBaseBendLine( un ),
  m_params( _pars )
{
  if ( un )
    m_params.RenameUnique( *un );
}


//------------------------------------------------------------------------------
// конструктор дублирования
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
  m_params( nullptr ) // параметры
{
}


I_IMP_SOMETHING_FUNCS_AR( ShMtBendLine )


//------------------------------------------------------------------------------
// запрос на получение интерфейса
// ---
// IfSomething * ShMtBendLine::QueryInterface ( uint iid ) {
//   return ShMtBaseBendLine::QueryInterface( iid );
// }
I_IMP_QUERY_INTERFACE( ShMtBendLine )
I_IMP_CASE_RI( CopibleObject );
I_IMP_CASE_RI( CopibleOperation );
I_IMP_END_QI_FROM_BASE( ShMtBaseBendLine );


//------------------------------------------------------------------------------
// Создать копию объекта - копию условного изображнния резьбы
// ---
IfPartObjectCopy * ShMtBendLine::CreateObjectCopy( IfComponent & compOwner ) const
{
  return ::CreateObjectCopy<PartObject, PartObjectCopy>( compOwner, *this );
}


//------------------------------------------------------------------------------
// Следует копировать на указанном компоненте
// ---
bool ShMtBendLine::NeedCopyOnComp( const IfComponent & component ) const
{
  return !!GetPartOperationResult().GetResult( component );
}


//------------------------------------------------------------------------------
// Можно ли копировать эту операцию как-нибудь иначе, кроме как геометрически?
// Т.е. через MakePureSolid или через интерфейсы IfCopibleOperationOnPlaces,
// IfCopibleOperationOnEdges и т.д.
// ---
bool ShMtBendLine::CanCopyNonGeometrically() const
{
  // Пока только геометрически.
  return false;
}


uint ShMtBendLine::GetObjType()  const { return ot_ShMtBendLine; } // вернуть тип объекта
uint ShMtBendLine::GetObjClass() const { return oc_ShMtBendLine; } // вернуть общий тип объекта
uint ShMtBendLine::OperationSubType() const { return bo_Union; }          // вернуть подтип операции

                                                                          //-----------------------------------------------------------------------------
                                                                          //
                                                                          //---
PrObject & ShMtBendLine::Duplicate() const
{
  return *new ShMtBendLine( *this );
}


//------------------------------------------------------------------------------
// задать параметры
// ---
void ShMtBendLine::SetParameters( const ShMtBaseBendParameters & p ) {
  m_params = (ShMtBendLineParameters&)p;
}


//------------------------------------------------------------------------------
// получить параметры
// ---
ShMtBaseBendParameters & ShMtBendLine::GetParameters() const {
  return const_cast<ShMtBendLineParameters&>(m_params);
}


//------------------------------------------------------------------------------
// перед тем как заканчиваем редактирование
// ---
void ShMtBendLine::EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp ) {
  IFPTR( ShMtBendLine ) ini( const_cast<IfSheetMetalOperation*>(&iniObj) );
  if ( ini ) {
    m_params.EditingEnd( ini->GetParameters(), *editComp.ReturnPartRebuildResult(), this );
  }
}


//------------------------------------------------------------------------------
// проверка определенности операции
// ---
bool ShMtBendLine::IsValid() const {
  bool res = ShMtBaseBendLine::IsValid();
  if ( res )
    res = m_params.IsValidParam();

  return res;
}


//------------------------------------------------------------------------------
// набирает массив предупреждений из ресурса
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
  return m_params.m_thickness;    // толщина
}


//---------------------------------------------------------------------------------
//
//---
void ShMtBendLine::SetThickness( double h ) {
  m_params.m_thickness = h;    // толщина
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendLine::GetOwnerAbsRadius() const {
  return m_params.m_internal ? m_params.GetRadius() : (m_params.GetRadius() - GetThickness());    // внутренний радиус сгиба
}


//---------------------------------------------------------------------------------
// Коэффициент, определяющий положение нейтрального слоя
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
  return  m_params.GetAbsAngle();     // угол сгиба 
}


//---------------------------------------------------------------------------------
// по внутреннему радиусу  
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
// если изменились какие либо параметры, и есть чтение коэф из базы, то прочитать базу 
//---
bool ShMtBendLine::IfOwnerBaseChanged( IfComponent * trans ) {
  return m_params.ReadBaseIfChanged( trans );
}


//---------------------------------------------------------------------------------
// дать тип развертки  
//---
UnfoldType ShMtBendLine::GetOwnerTypeUnfold() const {
  return m_params.m_typeUnfold;
}


//---------------------------------------------------------------------------------
// задать тип развертки  
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
// приготовить свои параметры
// ---
MbBendValues & ShMtBendLine::PrepareLocalParameters( const IfComponent * component ) {
  MbBendOverSegValues * params = new MbBendOverSegValues();
  IfComponent * comp = const_cast<IfComponent*>(component);
  m_params.ReadBaseIfChanged( comp ); // если параметры развертки брать из базы, то перечитать базу, если надо
  m_params.GetValues( *params );

  // CC K8 в старых версиях вычисление смещения производится сразу
  if ( GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0800000BL )
    m_params.CalcAfterValues( *params );

  return *params;
}


//------------------------------------------------------------------------------
// подготовиться к построению
//---
void ShMtBendLine::PrepareToBuild( const IfComponent & operOwner ) // компонент - владелец этой операции
{
  creatorBends = new CreatorForBendUnfoldParams( *this, (IfComponent*)&operOwner );
}


//------------------------------------------------------------------------------
// некие действия после построения
//---
void ShMtBendLine::PostBuild()
{
  delete creatorBends;
  creatorBends = nullptr;
}


//---------------------------------------------------------------------------------
// приготовить свое, характерное тело
//---
uint ShMtBendLine::PrepareLocalSolid( MbCurve3D            & curve,
  RPArray<MbFace>      & mbFaces,
  MbSNameMaker         & nameMaker,
  MbSolid              & prevSolid,
  MbBendValues         & params,    // свои уже приготовленные параметры
  const IfComponent    * operOwner, // компонент - владелец этой операции
  const IfComponent    * bodyOwner, // компонент - владелец тела, над которым эта операция производиться
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
      // CC K8 для новых версий вычисление смещения надо производить в самом конце
      if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
        m_params.CalcAfterValues( (MbBendOverSegValues&)params );

      if ( IsValidParamBaseTable() ) {
        codeError = ::BendSheetSolidOverSegment( prevSolid,                       // исходное тело
          wDone == wd_Phantom ? cm_Copy :
          cm_KeepHistory,                  // НГ работать с копией оболочки исходного тела
          mbFaces,                         // изгибаемая грань
          curve,                           // репер, в котором лежит отрезок
                                           /*Исправление ошибки 40270*/
          GetObjVersion().GetVersionContainer().GetAppVersion() > 0x0C000001L || GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0A000001L ? false : IsStraighten(),                  // гнуть
          (MbBendOverSegValues&)params,    // параметры листового тела
          nameMaker,                            // именователь
          result );                        // результирующее тело
                                           // надо запомнить все сгибы и их радиуса
        if ( creatorBends && wDone != wd_Phantom )
          creatorBends->SetResult( result );
      }
    }
  }

  return codeError;
}


//------------------------------------------------------------------------------
// Возможно ли для параметра задавать допуск
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
// Является ли параметр угловым
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
// Для параметра получить неуказанный предельный допуск
// ---
double ShMtBendLine::GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const
{
  if ( &varParameter == &const_cast<ShMtBendLineParameters&>(m_params).GetAngleVar() )
  {
    //    double angle = m_params.m_typeAngle ? m_params.GetAngle() : 180 - m_params.GetAngle();
    // CC K14 Для получения допуска используем радиус. Согласовано с Булгаковым
    return reader.GetTolerance( m_params.m_distance, // внешний радиус BUG 62631
      vd_angle,
      tType );
  }
  return ShMtBaseBend::GetGeneralTolerance( varParameter, tType, reader );
}


//------------------------------------------------------------------------------------
/// Расчет дуги для пересчета угловых параметров по допуску
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
    // У сгибов брать наименьший радиус
    double rad = MB_MAXDOUBLE;
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
    {
      double rad_i = m_bendObjects[i]->GetAbsRadius();
      if ( rad_i < rad )
        rad = rad_i;
    }

    m_params.m_distance = rad + GetThickness(); // внешний радиус BUG 62631
  }

  VariableParametersOwner::UpdateMeasurePars( gType, recalc );
}


// *** Хот-точки ***************************************************************
//------------------------------------------------------------------------------
// Создать хот-точки
// ---
void ShMtBendLine::GetHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  ShMtBaseBendLine::GetHotPoints( hotPoints, editComponent, topComponent, type, pD3DrawTool );

  hotPoints.Add( new HotPointBendLineAngleParam( ehp_ShMtBendAngle, editComponent, topComponent ) ); // угол сгиба
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
      CollectFaces( arrayFaces, editComponent, children ? children->m_extraName : -1 ); // собрать все грани
      if ( arrayFaces.Count() ) {
        params = static_cast<MbBendOverSegValues*>(&PrepareLocalParameters( &editComponent ));

        if ( children )
        {
          CreatorForBendUnfoldParams _creatorBends( *this, &editComponent );
          _creatorBends.Build( 1, children->m_extraName );
          _creatorBends.GetParams( *params, m_params.m_dirAngle );
        }

        if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
          // Смещение посчитать после всех вычислений
          static_cast<ShMtBendLineParameters&>(GetParameters()).CalcAfterValues( *params );

        // KA V17 Выбирать можно любые грани, в том числе на которых сгибы не создаются. Поэтому брать 0-выю грань для построения хт - нельзя
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
Пересчитать местоположение хот-точки
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
    // ША K11 6.2.2009 Ошибка №38858:
    // ША K11                                                    nullptr );
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
// Пересчитать местоположение хот-точек
/**
\param hotPoints - хот-точеки
\param children - сгиб, для которого определяем хот-точки
*/
//---
void ShMtBendLine::UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  ShMtBendObject &         children )
{
  bool del = true;

  // хот-точку радиуса отображаем только если сгиб выполнен по параметрам самого сгиба
  if ( !children.m_byOwner )
  {
    for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
    {
      HotPointParam * hotPoint = hotPoints[i];

      if ( hotPoint->GetIdent() == ehp_ShMtBendObjectRadius ) // радиус сгиба
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
// *** Хот-точки ***************************************************************


//------------------------------------------------------------------------------
/**
Могут ли к объекту быть привязаны управляющие размеры
\param  нет
\return нет
*/
//---

bool ShMtBendLine::IsDimensionsAllowed() const
{
  return true;
}


//------------------------------------------------------------------------------
/**
Обновить геометрию размеров объекта по хот-точкам
\param  dimensions - размеры
\param  hotPoints  - хот-точки
\return нет
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
Ассоциировать постоянные размеры с переменными родителя
\param  dimensions - размеры
\return нет
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
создать размеры объекта по хот-точкам для дочерних объектов
\param un -
\param dimensions -
\param hotPoints -
\return нет
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
обновить геометрию размеров объекта по хот-точкам для дочерних объектов
\param dimensions -
\param hotPoints -
\param children -
\return нет
*/
//---
void ShMtBendLine::UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
  const RPArray<HotPointParam> & hotPoints,
  ShMtBendObject & children )
{
  // хот-точку радиуса отображаем только если сгиб выполнен по параметрам самого сгиба
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
Ассоциировать размеры дочерних объектов - сгибов
\param dimensions -
\param children -
\return нет
*/
//---
void ShMtBendLine::AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
  ShMtBendObject        & children )
{
  ::UpdateDimensionVar( dimensions, ehp_ShMtBendObjectRadius, children.m_radius );
}


//-----------------------------------------------------------------------------
/// изменить уникальное имя параметра
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
Необходимо ли сохранение без истории
\param version
\return bool
*/
//---
bool ShMtBendLine::IsNeedSaveAsUnhistored( long version ) const
{
  return __super::IsSavedUnhistoried( version, m_params.m_typeRemoval ) || __super::IsNeedSaveAsUnhistored( version );
}


//------------------------------------------------------------------------------
// Чтение из потока
// ---
void ShMtBendLine::Read( reader &in, ShMtBendLine * obj ) {
  ReadBase( in, static_cast<ShMtBaseBendLine *>(obj) );
  in >> obj->m_params;  // параметры операции выдавливания
}


//------------------------------------------------------------------------------
// Запись в поток
// ---
void ShMtBendLine::Write( writer &out, const ShMtBendLine * obj ) {
  if ( out.AppVersion() < 0x06000033L )
    out.setState( io::cantWriteObject ); // специально введенный тип ошибочной ситуации
  else {
    WriteBase( out, static_cast<const ShMtBaseBendLine *>(obj) );
    out << obj->m_params;  // параметры операции выдавливания
  }
}


////////////////////////////////////////////////////////////////////////////////
//
// класса операций подсечка
//
////////////////////////////////////////////////////////////////////////////////
class ShMtBendHook : public ShMtBaseBendLine,
  public IfShMtBendHook {
public:
  ShMtBendHookParameters  m_params;
  ReferenceContainerFix   m_upToObject;      // объекты до которых строим

public:
  I_DECLARE_SOMETHING_FUNCS;

  // конструктор для использования в конструкторе процесса
  ShMtBendHook( const ShMtBendHookParameters &, IfUniqueName * un );
  // конструктор дублирования
  ShMtBendHook( const ShMtBendHook & );

public:
  virtual bool                     HasBendIgnoreOwner() const; // если true, то сгиб от этой операции может иметь параметры отличные от папы, даже если он создан по параметрам папы
#ifdef MANY_THICKNESS_BODY_SHMT
  virtual bool                     IsSuitedObject( const WrapSomething & );
#else
  virtual bool                     IsSuitedObject( const WrapSomething &, double );
#endif
  virtual double                   GetOwnerAbsAngle() const;
  virtual double                   GetOwnerValueBend() const;
  virtual double                   GetOwnerAbsRadius() const;    // внутренний радиус сгиба
  virtual bool                     IsOwnerInternalRad() const;    // по внутреннему радиусу?
  virtual UnfoldType               GetOwnerTypeUnfold() const;    // дать тип развертки  
  virtual void                     SetOwnerTypeUnfold( UnfoldType t );  // задать тип развертки  
  virtual bool                     GetOwnerFlagByBase() const;  // 
  virtual void                     SetOwnerFlagByBase( bool val );        // 
  virtual double                   GetOwnerCoefficient() const;  // коэффициент
  virtual bool                     IfOwnerBaseChanged( IfComponent * trans );        // если изменились какие либо параметры, и есть чтение коэф из базы, то прочитать базу 
  virtual double                   GetThickness() const;  // дать толщину
  virtual void                     SetThickness( double );// задать толщину      
                                                          /// перед тем как заканчиваем редактирование
  virtual void                     EditingEnd( const IfSheetMetalOperation & iniObj, const IfComponent & editComp );

  virtual void                     SetParameters( const ShMtBaseBendParameters & );
  virtual ShMtBaseBendParameters & GetParameters() const;
  virtual bool                     IsLeftSideFix();
  //--------------- общие функции объекта детали -------------------------------//
  virtual uint                     GetObjType() const;                            // вернуть тип объекта
  virtual uint                     GetObjClass() const;                            // вернуть общий тип объекта
  virtual PrObject               & Duplicate() const;

  virtual bool                     RestoreReferences( PartRebuildResult &result, bool allRefs, CERst withMath ); // восстановить связи
  virtual void                     ClearReferencedObjects( bool soft );
  virtual bool                     ChangeReferences( ChangeReferencesData & changedRefs ); ///< Заменить ссылки на объекты
  virtual bool                     IsValid() const;  // проверка определенности объекта
  virtual void                     WhatsWrong( WarningsArray & ); // набирает массив предупреждений из ресурса
  virtual bool                     IsThisParent( uint8 relType, const WrapSomething & ) const;
  virtual void                     PrepareToBuild( const IfComponent & ); ///< подготовиться к построению
  virtual void                     PostBuild(); ///< некие действия после построения
  virtual bool                     CopyInnerProps( const IfPartObject& source,
    uint sourceSubObjectIndex,
    uint destSubObjectIndex ) override;
  /// Надо ли сохранять как объект без истории
  virtual bool                     IsNeedSaveAsUnhistored( long version ) const override;

  /// приготовить свое, характерное тело
  virtual uint            PrepareLocalSolid( MbCurve3D            & curve,
    RPArray<MbFace>      & mbFaces,
    MbSNameMaker         & name,
    MbSolid              & prevSolid,
    MbBendValues         & params,        // свои уже приготовленные параметры
    const IfComponent    * /*operOwner*/, // компонент - владелец этой операции
    const IfComponent    * /*bodyOwner*/, // компонент - владелец тела, над которым эта операция производиться
    WhatIsDone             wDone,
    uint                   idShellThread,
    MbSolid              *&currSolid );
  // приготовить свои параметры
  virtual MbBendValues &  PrepareLocalParameters( const IfComponent * component );

  // добавить или удалить UptoObject
  virtual bool                  SetResetUpToObject( const WrapSomething & );
  virtual const WrapSomething & GetUpToObject() const;
  // удалить UptoObject
  virtual void                  ResetUpToObject();

  /// Возможно ли для параметра задавать допуск
  virtual bool                  IsCanSetTolerance( const VarParameter & varParameter ) const;
  /// Для параметра получить неуказанный предельный допуск
  virtual double                GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const;
  /// Является ли параметр угловым
  virtual bool                  ParameterIsAngular( const VarParameter & varParameter ) const;

  // Реализация IfHotPoints
  /// Создать хот-точки               
  virtual void GetHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool );
  /// Пересчитать местоположение хот-точки
  virtual void UpdateHotPoints( RPArray<HotPointParam> & hotPoints,
    ActiveHotPoints          type,
    D3Draw &                 pD3DrawTool );
  /// Перерасчет при перетаскивании хот-точки 
  // step - шаг дискретности, для определения смещения с заданным пользователем шагом
  virtual bool ChangeHotPoint( HotPointParam & hotPoint, const MbVector3D & vector,
    double step, D3Draw & pD3DrawTool );
  /// Пересчитать местоположение хот-точек
  virtual void UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    ShMtBendObject &         children ) override;

  // Реализация хот-точек радиусов сгибов от ShMtStraighten
  /// Создать хот-точки (зарезервированы индексы с 5000 до 5999)
  virtual void GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
    IfComponent &            editComponent,
    IfComponent &            topComponent,
    ShMtBendObject &         children );
  /// Перерасчет при перетаскивании хот-точки
  virtual bool ChangeChildrenHotPoint( HotPointParam &  hotPoint,
    const MbVector3D &     vector,
    double           step,
    ShMtBendObject & children );
  /// изменить уникальное имя параметра
  virtual void RenameUnique( IfUniqueName & un );

  /// могут ли к объекту быть привязаны управляющие размеры
  virtual bool IsDimensionsAllowed() const;
  /// ассоциировать размеры с переменными родителя
  virtual void AssignDimensionsVariables( const SFDPArray<PartObject> & dimensions );
  /// создать размеры объекта по хот-точкам
  virtual void CreateDimensionsByHotPoints( IfUniqueName * un,
    SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints );
  /// обновить геометрию размеров объекта по хот-точкам
  virtual void UpdateDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    const PArray<OperationSpecificDispositionVariant>* pVariants );

  /// Ассоциировать размеры дочерних объектов - сгибов
  virtual void AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
    ShMtBendObject & children );
  /// создать размеры объекта по хот-точкам для дочерних объектов
  virtual void CreateChildrenDimensionsByHotPoints( IfUniqueName * un,
    SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children );
  /// обновить геометрию размеров объекта по хот-точкам для дочерних объектов
  virtual void UpdateChildrenDimensionsByHotPoints( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    ShMtBendObject & children );
  /// Расчет дуги для пересчета угловых параметров по допуску
  virtual void UpdateMeasurePars( GeneralToleranceType gType, bool recalc );

private:
  std::unique_ptr<HotPointBendHookOperUtil> GetHotPointUtil( MbJogValues   *& params,
    MbBendValues   & secondBendParams,
    IfComponent    & editComponent,
    ShMtBendObject * children );

  /// Спрятать хот-точки
  void HideAllHotPoints( RPArray<HotPointParam> & hotPoints );

  /// Обновить хот-точку высоты
  void UpdateDistanceExtrHotPoint( RPArray<HotPointParam> & hotPoints,
    HotPointBendHookOperUtil & util,
    MbBendValues secondBendParameters );
  /// Обновить хот-точку радиуса, установив её на сгибе созданном по исходному объекту
  void UpdateRadiusHotPointByBendObjects( RPArray<HotPointParam> & hotPoints,
    HotPointBendHookOperUtil & util,
    MbBendOverSegValues & bendParameters );
  /// Следует ли производить расчеты хот-точки радиуса на первом сгибе
  bool IsRadiusHotPointByFirstBend();
  /// Следует ли производить расчеты хот-точки радиуса на втором сгибе
  bool IsRadiusHotPointBySecondBend();
  /// Произвести перерасчет при перетаскивании хот-точки радиуса, в зависимости от 
  /// построения сгибов по исходному объекту или нет
  bool ChangeRadiusHotPointByBendObjects( HotPointParam & radiusHotPoint, const MbVector3D & vector,
    double step, D3Draw & pD3DrawTool );

  /// Обновить размер расстояния
  void UpdateLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints );
  /// Обновить размер радиуса
  void UpdateRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints,
    int radiusHotPointId,
    EDimensionsIds radiusDimensionId,
    bool moveToInternal );
  /// Обновить размер угла
  void UpdateAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions,
    const RPArray<HotPointParam> & hotPoints );

  /// Получиь параметры размера угла
  bool GetAngleDimensionParameters( const RPArray<HotPointParam> & hotPoints,
    MbCartPoint3D & dimensionCenter,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// Получить параметры размера расстояния - снаружи
  bool GetLengthDimensionParametersByOut( const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// Получить параметры размера расстояния - внутри
  bool GetLengthDimensionParametersByIn( const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// Получить параметры размера расстояния - полный
  bool GetLengthDimensionParametersByAll( const RPArray<HotPointParam> & hotPoints,
    MbPlacement3D & dimensionPlacement,
    MbCartPoint3D & dimensionPoint1,
    MbCartPoint3D & dimensionPoint2 );
  /// Обнулить размер длинны
  void ResetLengthDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions );
  /// Обнулить размер длинны
  void ResetAngleDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions );
  /// Обнулить размер радиуса
  void ResetRadiusDimension( const SFDPArray<GenerativeDimensionDescriptor> & dimensions, EDimensionsIds radiusDimensionId );

  bool GetRadiusDimensionParameters( HotPointBendLineRadiusParam * radiusHotPoint, bool moveToInternal,
    MbPlacement3D & dimensionPlacement, MbArc3D & dimensionArc );

private:
  void operator = ( const ShMtBendHook & ); // не реализовано

  DECLARE_PERSISTENT_CLASS( ShMtBendHook )
};



//------------------------------------------------------------------------------
// конструктор, просто запоминает параметры ничего не рассчитывая, эскиз дадим потом
// конструктор для использования в конструкторе процесса
ShMtBendHook::ShMtBendHook( const ShMtBendHookParameters & _pars, IfUniqueName * un )
  : ShMtBaseBendLine( un ),
  m_upToObject( 1 ),
  m_params( _pars )
{
  if ( un )
    m_params.RenameUnique( *un );
}


//------------------------------------------------------------------------------
// конструктор дублирования
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
  m_params( nullptr ) // параметры
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


uint ShMtBendHook::GetObjType()  const { return ot_ShMtBendHook; } // вернуть тип объекта
uint ShMtBendHook::GetObjClass() const { return oc_ShMtBendHook; } // вернуть общий тип объекта

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
// задать параметры
// ---
void ShMtBendHook::SetParameters( const ShMtBaseBendParameters & p ) {
  m_params = (ShMtBendHookParameters&)p;
}


//------------------------------------------------------------------------------
// получить параметры
// ---
ShMtBaseBendParameters & ShMtBendHook::GetParameters() const {
  return const_cast<ShMtBendHookParameters&>(m_params);
}


//------------------------------------------------------------------------------
// проверка определенности операции
// ---
bool ShMtBendHook::IsValid() const {
  bool res = ShMtBaseBendLine::IsValid() && m_params.IsValidParam();
  if ( res )
    if ( m_params.m_typeExtrusion != ShMtBendHookParameters::be_blind )
      res = !m_upToObject.IsEmpty();

  return res;
}


//------------------------------------------------------------------------------
// восстановить связи
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
  // Очистить опорные объекты soft = true - только враперы (не трогая имена)
  // ---
void ShMtBendHook::ClearReferencedObjects( bool soft )
{
  ShMtBaseBendLine::ClearReferencedObjects( soft );
  m_upToObject.ClearObjects( soft );
}


//------------------------------------------------------------------------------
// Заменить ссылки на объекты
// ---
bool ShMtBendHook::ChangeReferences( ChangeReferencesData & changedRefs )
{
  bool res = ShMtBaseBendLine::ChangeReferences( changedRefs );
  if ( m_upToObject.ChangeReferences( changedRefs ) )
    res = true;

  return res;
}


//--------------------------------------------------------------------------------
// добавить или удалить UptoObject
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
// удалить UptoObject
//---
void ShMtBendHook::ResetUpToObject() {
  m_upToObject.SetObject( 0, nullptr );
}


//------------------------------------------------------------------------------
// набирает массив предупреждений из ресурса
// ---
void ShMtBendHook::WhatsWrong( WarningsArray & warnings ) {
  ShMtBaseBendLine::WhatsWrong( warnings );

  if ( m_params.m_typeExtrusion != ShMtBendHookParameters::be_blind && m_upToObject.IsEmpty() )
    m_upToObject.WhatsWrong( warnings );

  m_params.WhatsWrongParam( warnings );

  bool yes = true;
  for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ ) {
    if ( !m_params.m_typeAngle && ::fabs( m_bendObjects[i]->GetAbsAngle() ) + PARAM_NEAR >= M_PI ) {
      // Дополняющий угол должен быть меньше 360 градусов.
      warnings.AddWarning( IDP_SHMT_ERROR_BEND_HOOK_ADDED_ANGLE, module );
      yes = false;
      break;
    }
  }

  if ( yes )
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ ) {
      // угол должен быть меньше 180 гр.
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
// перед тем как заканчиваем редактирование
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
  return m_params.m_thickness;    // толщина
}


//---------------------------------------------------------------------------------
//
//---
void ShMtBendHook::SetThickness( double h ) {
  m_params.m_thickness = h;    // толщина
}


//---------------------------------------------------------------------------------
//
//---
double ShMtBendHook::GetOwnerAbsRadius() const {
  //	return m_params.GetRadius();    // радиус сгиба
  return m_params.m_internal ? m_params.GetRadius() : (m_params.GetRadius() - GetThickness());    // внутренний радиус сгиба
}


//---------------------------------------------------------------------------------
// Коэффициент, определяющий положение нейтрального слоя
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
// по внутреннему радиусу  
//---
bool ShMtBendHook::IsOwnerInternalRad() const {
  return m_params.m_internal;
}


//---------------------------------------------------------------------------------
// дать тип развертки  
//---
UnfoldType ShMtBendHook::GetOwnerTypeUnfold() const {
  return m_params.m_typeUnfold;
}


//---------------------------------------------------------------------------------
// задать тип развертки  
//---
void ShMtBendHook::SetOwnerTypeUnfold( UnfoldType t ) {
  m_params.m_typeUnfold = t;
}


//------------------------------------------------------------------------
// если true, то сгиб от этой операции может иметь параметры отличные от папы, даже если он создан по параметрам папы
//---
bool ShMtBendHook::HasBendIgnoreOwner() const {
  bool newAngle = false;
  if ( m_bendObjects.Count() == 2 ) {
    // если ввысота подсечки меньше минимальной, то надо уменьшить угол
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
// если изменились какие либо параметры, и есть чтение коэф из базы, то прочитать базу 
//---
bool ShMtBendHook::IfOwnerBaseChanged( IfComponent * trans ) {
  return m_params.ReadBaseIfChanged( trans );
}


//------------------------------------------------------------------------------
// годится ли пришедший объект
// ---
#ifdef MANY_THICKNESS_BODY_SHMT
bool ShMtBendHook::IsSuitedObject( const WrapSomething & wrapper )
#else
bool ShMtBendHook::IsSuitedObject( const WrapSomething & wrapper, double thickness )
#endif
{
  bool res = false;
  // ША K11 17.12.2008 Ошибка №37142: Запрещяем выбор ребер многочастных тел
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
// подготовиться к построению
//---
void ShMtBendHook::PrepareToBuild( const IfComponent & operOwner ) // компонент - владелец этой операции
{
  creatorBendHooks = new CreatorForBendHookUnfoldParams( *this, (IfComponent*)&operOwner );
}


//------------------------------------------------------------------------------
// некие действия после построения
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
// приготовить свои параметры
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
          m_params.ReadBaseIfChanged( const_cast<IfComponent*>(component) ); // если параметры развертки брать из базы, то перечитать базу, если надо
          m_params.GetValues( *params );

          // CC K8 в старых версиях вычисление смещения производится сразу
          if ( GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0800000BL )
            m_params.CalcAfterValues( *params );

        }
      }
    }
  }
  return *params;
}


//---------------------------------------------------------------------------------
// приготовить свое, характерное тело
//---
uint ShMtBendHook::PrepareLocalSolid( MbCurve3D            & curve,
  RPArray<MbFace>      & mbFaces,
  MbSNameMaker         & nameMaker,
  MbSolid              & prevSolid,
  MbBendValues         & params,    // свои уже приготовленные параметры
  const IfComponent    * operOwner, // компонент - владелец этой операции
  const IfComponent    * bodyOwner, // компонент - владелец тела, над которым эта операция производиться
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

    // CC K8 для новых версий вычисление смещения надо производить в самом конце
    if ( GetObjVersion().GetVersionContainer().GetAppVersion() >= 0x0800000BL )
      m_params.CalcAfterValues( (MbBendOverSegValues&)params );

    bool yes = true;
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
    {
      // угол должен быть меньше 180 гр.
      if ( ::fabs( m_bendObjects[i]->GetAbsAngle() ) + PARAM_NEAR >= M_PI )
      {
        yes = false;
        codeError = rt_Error;
        break;
      }
    }


    if ( yes && IsValidParamBaseTable() )
    {
      PArray<MbFace> par_1( 0, 1, false ); // грани первого сгиба подсечки
      PArray<MbFace> par_2( 0, 1, false ); // грани второго сгиба подсечки

      codeError = ::SheetSolidJog( prevSolid,                                   // исходное тело
        wDone == wd_Phantom ? cm_Copy
        : cm_KeepHistory,        // НГ работать с копией оболочки исходного тела
        mbFaces,                                     // изгибаемые грани
        curve,                                       // прямолинейная кривая, вдоль которой гнуть
                                                     /*Исправление ошибки 40270*/
        GetObjVersion().GetVersionContainer().GetAppVersion() > 0x0C000001L || GetObjVersion().GetVersionContainer().GetAppVersion() < 0x0A000001L ? false : IsStraighten(),                  // гнуть
        (MbJogValues&)params,                        // параметры листового тела
        secondBendParams,                            // параметры второго сгиба
        nameMaker,                                        // именователь
        par_1,                                       // грани первого сгиба подсечки
        par_2,                                       // грани второго сгиба подсечки
        result );                                    // результирующее тело

      if ( creatorBendHooks && wDone != wd_Phantom )
      {
        creatorBendHooks->SetResult( result );
        if ( result && codeError == rt_Success )
          // надо запомнить все сгибы и их радиуса
          creatorBendHooks->InitFaces( par_1, par_2, MainId(), GetObjVersion() );
      }
    }
  }

  return codeError;
}


//------------------------------------------------------------------------------
// Возможно ли для параметра задавать допуск
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
// Является ли параметр угловым
// ---
bool ShMtBendHook::ParameterIsAngular( const VarParameter & varParameter ) const
{
  if ( &varParameter == &const_cast<ShMtBendHookParameters&>(m_params).GetAngleVar() )
    return  true;

  return false;
}


//------------------------------------------------------------------------------
// Для параметра получить неуказанный предельный допуск
// ---
double ShMtBendHook::GetGeneralTolerance( const VarParameter & varParameter, GeneralToleranceType tType, ItToleranceReader & reader ) const
{
  if ( &varParameter == &const_cast<ShMtBendHookParameters&>(m_params).GetAngleVar() )
  {
    //    double angle = m_params.m_typeAngle ? m_params.GetAngle() : 180 - m_params.GetAngle();
    // CC K14 Для получения допуска используем радиус. Согласовано с Булгаковым
    return reader.GetTolerance( m_params.m_distance, // внешний радиус BUG 62631
      vd_angle,
      tType );
  }
  return ShMtBaseBend::GetGeneralTolerance( varParameter, tType, reader );
}


//------------------------------------------------------------------------------------
/// Расчет дуги для пересчета угловых параметров по допуску
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
    // У сгибов брать наименьший радиус
    double rad = MB_MAXDOUBLE;
    for ( size_t i = 0, c = m_bendObjects.Count(); i < c; i++ )
    {
      double rad_i = m_bendObjects[i]->GetAbsRadius();
      if ( rad_i < rad )
        rad = rad_i;
    }

    m_params.m_distance = rad + GetThickness(); // внешний радиус BUG 62631
  }

  VariableParametersOwner::UpdateMeasurePars( gType, recalc );
}


// *** Хот-точки ***************************************************************
//------------------------------------------------------------------------------
// Создать хот-точки
// ---
void ShMtBendHook::GetHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ActiveHotPoints          type,
  D3Draw &                 pD3DrawTool )
{
  ShMtBaseBendLine::GetHotPoints( hotPoints, editComponent, topComponent, type, pD3DrawTool );

  hotPoints.Add( new HotPointHookHeightParam( ehp_ShMtBendDistanceExtr, editComponent, topComponent ) ); // высота 
  hotPoints.Add( new HotPointBendAngleParam( ehp_ShMtBendAngle, editComponent, topComponent ) ); // угол сгиба
}


//-----------------------------------------------------------------------------
// Перерасчет при перетаскивании хот-точки 
/**
\param hotPoint - сдвинутая хот-точка
\param vector - вектор сдвига
\param step - шаг дискретности, для определения смещения с заданным пользователем шагом
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
Спрятать хот-точки
\param hotPoints - массив хот-точек
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
Обновить хот-точку высоты
\param hotPoints - массив хот-точек
\param util - утилита для расчета хот-точек
\param secondBendParameters - параметры сгиба
\param drawTool - инструмент рисования
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
Следует ли производить расчеты хот-точки радиуса на втором сгибе
\return Следует ли производить расчеты хот-точки радиуса на втором сгибе
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
Следует ли производить расчеты хот-точки радиуса на первом сгибе
\return Следует ли производить расчеты хот-точки радиуса на первом сгибе
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
Обновить хот-точку радиуса, установив её на сгибе созданном по исходному объекту
\param hotPoints - массив хот-точек
\param util - утилина для расчета хот-точек
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
Пересчитать местоположение хот-точки
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
      CollectFaces( arrayFaces, editComponent, children ? children->m_extraName : -1 ); // собрать все грани
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
          // Смещение посчитать после всех вычислений
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
// Создать хот-точки (зарезервированы индексы с 5000 до 5999)
// ---
void ShMtBendHook::GetChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  IfComponent &            editComponent,
  IfComponent &            topComponent,
  ShMtBendObject &         children )
{
  hotPoints.Add( new HotPointBendHookObjectRadiusParam( ehp_ShMtBendObjectRadius,
    editComponent, topComponent ) ); // радиус сгиба 
}


//-----------------------------------------------------------------------------
// Перерасчет при перетаскивании хот-точки 
/**
\param hotPoint - перетаскиваемая хот-точка
\param vector - вектор смещения
\param step - шаг дискретности, для определения смещения с заданным пользователем шагом
\param children - сгиб, для которого определяем хот-точки
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
    if ( hotPoint.GetIdent() == ehp_ShMtBendObjectRadius ) // радиус сгиба
      ret = ((HotPointBendHookObjectRadiusParam &)hotPoint).SetRadiusHotPoint_2( vector, step,
        GetParameters(),
        children );
  }

  return ret;
}


//-----------------------------------------------------------------------------
/// изменить уникальное имя параметра
//---
void ShMtBendHook::RenameUnique( IfUniqueName & un ) {
  ShMtBaseBendLine::RenameUnique( un );
  m_params.RenameUnique( un );
}


//------------------------------------------------------------------------------
/**
могут ли к объекту быть привязаны управляющие размеры
\return могут ли к объекту быть привязаны управляющие размеры
*/
//--- 
bool ShMtBendHook::IsDimensionsAllowed() const
{
  return true;
}


//------------------------------------------------------------------------------
/**
ассоциировать размеры с переменными родителя
\param dimensions - размеры
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
создать размеры объекта по хот-точкам
\param un -
\param dimensions - размеры
\param hotPoints - хот-точки
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
Ассоциировать размеры дочерних объектов - сгибов
\param dimensions - размеры
\param children - дочерний сгиб
*/
//--- 
void ShMtBendHook::AssingChildrenDimensionsVariables( const SFDPArray<PartObject> & dimensions,
  ShMtBendObject & children )
{
  ::UpdateDimensionVar( dimensions, ed_childrenRadius, children.m_radius );
}


//------------------------------------------------------------------------------
/**
создать размеры объекта по хот-точкам для дочерних объектов
\param un -
\param dimensions - размеры
\param hotPoints - хот-точки
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
обновить геометрию размеров объекта по хот-точкам для дочерних объектов
\param dimensions - размеры
\param hotPoints - хот-точки
\param children - дочерний сгиб
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
обновить геометрию размеров объекта по хот-точкам
\param dimensions - размеры
\param hotPoints - хот-точки
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
Обнулить размер длинны
\param dimensions - размеры
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
Обнулить размер длинны
\param dimensions - размеры
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
Обнулить размер радиуса
\param dimensions - размеры
\param radiusDimensionId - id Размера
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
Обновить размер угла сгиба
\param pDimensions - размеры операции
\param pUtils - параметры хот-точек
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
Получиь параметры размера угла
\param hotPoints - хот-точки
\param dimensionCenter - центр размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
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
Обновить размер радиуса
\param dimensions - размеры
\param hotPoints - хот-точки
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
Получить параметры размера радиуса
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

  // перемещение на внутренний радиус
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
Обновить размер расстояния
\param dimensions - размеры
\param hotPoints - хот-точки
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
Получить параметры размера расстояния - снаружи
\param hotPoints - хот-точки
\param dimensionPlacement - положение размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
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
Получить параметры размера расстояния - внутри
\param hotPoints - хот-точки
\param dimensionPlacement - положение размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
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
Получить параметры размера расстояния - полный
\param hotPoints - хот-точки
\param dimensionPlacement - положение размера
\param dimensionPoint1 - точка 1 размера
\param dimensionPoint2 - точка 2 размера
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
// Пересчитать местоположение хот-точек
/**
\param hotPoints - хот-точеки
\param children - сгиб, для которого определяем хот-точки
*/
//---
void ShMtBendHook::UpdateChildrenHotPoints( RPArray<HotPointParam> & hotPoints,
  ShMtBendObject &         children )
{
  bool del = true;

  if ( !children.m_byOwner )
  {
    // хот-точку радиуса отображаем только если сгиб выполнен по параметрам самого сгиба
    for ( size_t i = 0, c = hotPoints.Count(); i < c; i++ )
    {
      HotPointParam * hotPoint = hotPoints[i];
      if ( hotPoint->GetIdent() == ehp_ShMtBendObjectRadius ) // радиус сгиба
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

          if ( children.m_ind % 2 == 0 ) // четное
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
// *** Хот-точки ***************************************************************


IMP_PERSISTENT_CLASS( kompasDeprecatedAppID, ShMtBendHook );
IMP_PROC_REGISTRATION( ShMtBendHook );


//--------------------------------------------------------------------------------
/**
Необходимо ли сохранение без истории
\param version
\return bool
*/
//---
bool ShMtBendHook::IsNeedSaveAsUnhistored( long version ) const
{
  return __super::IsSavedUnhistoried( version, m_params.m_typeRemoval ) || __super::IsNeedSaveAsUnhistored( version );
}


//------------------------------------------------------------------------------
// Чтение из потока 
// ---
void ShMtBendHook::Read( reader &in, ShMtBendHook * obj ) {
  ReadBase( in, static_cast<ShMtBaseBendLine *>(obj) );
  in >> obj->m_upToObject;
  in >> obj->m_params;
}


//------------------------------------------------------------------------------
// Запись в поток
// ---
void ShMtBendHook::Write( writer &out, const ShMtBendHook * obj ) {
  if ( out.AppVersion() < 0x0700010FL )
    out.setState( io::cantWriteObject ); // специально введенный тип ошибочной ситуации
  else {
    WriteBase( out, static_cast<const ShMtBaseBendLine *>(obj) );
    out << obj->m_upToObject;
    out << obj->m_params;
  }
}


//------------------------------------------------------------------------------
// Сгиб
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
// Сгиб по линии
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
// Подсечка
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




