/***************************************************************************
    qgsgeometrycheck.h
    ---------------------
    begin                : September 2014
    copyright            : (C) 2014 by Sandro Mani / Sourcepole AG
    email                : smani at sourcepole dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#define SIP_NO_FILE

#ifndef QGS_GEOMETRY_CHECK_H
#define QGS_GEOMETRY_CHECK_H

#include <QApplication>
#include <limits>
#include <QStringList>
#include <QPointer>

#include "qgis_analysis.h"
#include "qgsfeature.h"
#include "qgsvectorlayer.h"
#include "geometry/qgsgeometry.h"
#include "qgsgeometrycheckerutils.h"

class QgsGeometryCheckError;
class QgsFeaturePool;

#define FEATUREID_NULL std::numeric_limits<QgsFeatureId>::min()

struct ANALYSIS_EXPORT QgsGeometryCheckerContext
{
    QgsGeometryCheckerContext( int _precision, const QgsCoordinateReferenceSystem &_mapCrs, const QMap<QString, QgsFeaturePool *> &_featurePools, const QgsCoordinateTransformContext &transformContext );
    const double tolerance;
    const double reducedTolerance;
    const QgsCoordinateReferenceSystem mapCrs;
    const QMap<QString, QgsFeaturePool *> featurePools;
    const QgsCoordinateTransformContext transformContext;
    const QgsCoordinateTransform &layerTransform( const QPointer<QgsVectorLayer> &layer );
    double layerScaleFactor( const QPointer<QgsVectorLayer> &layer );

  private:
    QMap<QPointer<QgsVectorLayer>, QgsCoordinateTransform> mTransformCache;
    QMap<QPointer<QgsVectorLayer>, double> mScaleFactorCache;
    QReadWriteLock mCacheLock;
};

class ANALYSIS_EXPORT QgsGeometryCheck
{
    Q_DECLARE_TR_FUNCTIONS( QgsGeometryCheck )

  public:
    enum ChangeWhat
    {
      ChangeFeature,
      ChangePart,
      ChangeRing,
      ChangeNode
    };

    enum ChangeType
    {
      ChangeAdded,
      ChangeRemoved,
      ChangeChanged
    };

    enum CheckType
    {
      FeatureNodeCheck,
      FeatureCheck,
      LayerCheck
    };

    struct Change
    {
      Change() = default;
      Change( ChangeWhat _what, ChangeType _type, QgsVertexId _vidx = QgsVertexId() )
        : what( _what )
        , type( _type )
        , vidx( _vidx )
      {}
      ChangeWhat what;
      ChangeType type;
      QgsVertexId vidx;
      bool operator==( const Change &other )
      {
        return what == other.what && type == other.type && vidx == other.vidx;
      }
    };

    typedef QMap<QString, QMap<QgsFeatureId, QList<Change>>> Changes;

    QgsGeometryCheck( CheckType checkType, const QList<QgsWkbTypes::GeometryType> &compatibleGeometryTypes, QgsGeometryCheckerContext *context )
      : mCheckType( checkType )
      , mCompatibleGeometryTypes( compatibleGeometryTypes )
      , mContext( context )
    {}
    virtual ~QgsGeometryCheck() = default;
    virtual void collectErrors( QList<QgsGeometryCheckError *> &errors, QStringList &messages, QAtomicInt *progressCounter = nullptr, const QMap<QString, QgsFeatureIds> &ids = QMap<QString, QgsFeatureIds>() ) const = 0;
    virtual void fixError( QgsGeometryCheckError *error, int method, const QMap<QString, int> &mergeAttributeIndices, Changes &changes ) const = 0;
    virtual QStringList resolutionMethods() const = 0;
    virtual QString errorDescription() const = 0;
    virtual QString errorName() const = 0;
    CheckType checkType() const { return mCheckType; }
    bool isCompatible( QgsWkbTypes::GeometryType type ) const { return mCompatibleGeometryTypes.contains( type ); }
    QgsGeometryCheckerContext *context() const { return mContext; }

  protected:
    QMap<QString, QgsFeatureIds> allLayerFeatureIds() const;
    void replaceFeatureGeometryPart( const QString &layerId, QgsFeature &feature, int partIdx, QgsAbstractGeometry *newPartGeom, Changes &changes ) const;
    void deleteFeatureGeometryPart( const QString &layerId, QgsFeature &feature, int partIdx, Changes &changes ) const;
    void deleteFeatureGeometryRing( const QString &layerId, QgsFeature &feature, int partIdx, int ringIdx, Changes &changes ) const;

    const CheckType mCheckType;
    QList<QgsWkbTypes::GeometryType> mCompatibleGeometryTypes;
    QgsGeometryCheckerContext *mContext;
};


class ANALYSIS_EXPORT QgsGeometryCheckError
{
  public:
    enum Status { StatusPending, StatusFixFailed, StatusFixed, StatusObsolete };
    enum ValueType { ValueLength, ValueArea, ValueOther };

    QgsGeometryCheckError( const QgsGeometryCheck *check,
                           const QgsGeometryCheckerUtils::LayerFeature &layerFeature,
                           const QgsPointXY &errorLocation,
                           QgsVertexId vidx = QgsVertexId(),
                           const QVariant &value = QVariant(),
                           ValueType valueType = ValueOther );

    virtual ~QgsGeometryCheckError() = default;

    const QgsGeometryCheckError &operator=( const QgsGeometryCheckError & ) = delete;

    const QgsGeometryCheck *check() const { return mCheck; }
    const QString &layerId() const { return mLayerId; }
    QgsFeatureId featureId() const { return mFeatureId; }
    // In map units
    const QgsAbstractGeometry *geometry() const;
    // In map units
    virtual QgsRectangle affectedAreaBBox() const;
    virtual QString description() const { return mCheck->errorDescription(); }
    // In map units
    const QgsPointXY &location() const { return mErrorLocation; }
    // Lengths, areas in map units
    QVariant value() const { return mValue; }
    ValueType valueType() const { return mValueType; }
    const QgsVertexId &vidx() const { return mVidx; }
    Status status() const { return mStatus; }
    QString resolutionMessage() const { return mResolutionMessage; }
    void setFixed( int method );
    void setFixFailed( const QString &reason );
    void setObsolete() { mStatus = StatusObsolete; }

    /**
     * Check if this error is equal to \a other.
     * Is reimplemented by subclasses with additional information, comparison
     * of base information is done in parent class.
     */
    virtual bool isEqual( QgsGeometryCheckError *other ) const;

    /**
     * Check if this error is almost equal to \a other.
     * If this returns true, it can be used to update existing errors after re-checking.
     */
    virtual bool closeMatch( QgsGeometryCheckError * /*other*/ ) const;

    /**
     * Update this error with the information from \other.
     * Will be used to update existing errors whenever they are re-checked.
     */
    virtual void update( const QgsGeometryCheckError *other );

    /**
     * Apply a list of \a changes.
     */
    virtual bool handleChanges( const QgsGeometryCheck::Changes &changes );

  protected:
    // Users of this constructor must ensure geometry and errorLocation are in map coordinates
    QgsGeometryCheckError( const QgsGeometryCheck *check,
                           const QString &layerId,
                           QgsFeatureId featureId,
                           const QgsGeometry &geometry,
                           const QgsPointXY &errorLocation,
                           QgsVertexId vidx = QgsVertexId(),
                           const QVariant &value = QVariant(),
                           ValueType valueType = ValueOther );

    const QgsGeometryCheck *mCheck = nullptr;
    QString mLayerId;
    QgsFeatureId mFeatureId;
    QgsGeometry mGeometry;
    QgsPointXY mErrorLocation;
    QgsVertexId mVidx;
    QVariant mValue;
    ValueType mValueType;
    Status mStatus;
    QString mResolutionMessage;

};

Q_DECLARE_METATYPE( QgsGeometryCheckError * )

#endif // QGS_GEOMETRY_CHECK_H
