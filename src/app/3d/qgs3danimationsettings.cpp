/***************************************************************************
  qgs3danimationsettings.cpp
  --------------------------------------
  Date                 : July 2018
  Copyright            : (C) 2018 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgs3danimationsettings.h"

#include <QEasingCurve>
#include <QDomDocument>

float Qgs3DAnimationSettings::duration() const
{
  return mKeyframes.isEmpty() ? 0 : mKeyframes.constLast().time;
}

Qgs3DAnimationSettings::Keyframe Qgs3DAnimationSettings::interpolate( float time ) const
{
  if ( mKeyframes.isEmpty() )
    return Keyframe();

  if ( time < mKeyframes.constFirst().time )
  {
    return mKeyframes.first();
  }
  else if ( time >= mKeyframes.constLast().time )
  {
    return mKeyframes.last();
  }
  else
  {
    // TODO: make easing curves configurable.
    // QEasingCurve is probably not flexible enough, we may need more granular
    // control with Bezier curves to allow smooth transition at keyframes

    for ( int i = 0; i < mKeyframes.size() - 1; i++ )
    {
      const Keyframe &k0 = mKeyframes.at( i );
      const Keyframe &k1 = mKeyframes.at( i + 1 );
      if ( time >= k0.time && time <= k1.time )
      {
        float ip = ( time - k0.time ) / ( k1.time - k0.time );
        float eIp = mEasingCurve.valueForProgress( ip );
        float eIip = 1.0f - eIp;

        Keyframe kf;
        kf.time = time;
        kf.point.set( k0.point.x() * eIip + k1.point.x() * eIp,
                      k0.point.y() * eIip + k1.point.y() * eIp,
                      k0.point.z() * eIip + k1.point.z() * eIp );
        kf.dist = k0.dist * eIip + k1.dist * eIp;
        kf.pitch = k0.pitch * eIip + k1.pitch * eIp;

        // always use shorter angle
        float yaw0 = fmod( k0.yaw, 360 ), yaw1 = fmod( k1.yaw, 360 );
        if ( std::abs( yaw0 - yaw1 ) > 180 )
        {
          if ( yaw0 < yaw1 )
            yaw0 += 360;
          else
            yaw1 += 360;
        }

        kf.yaw = yaw0 * eIip + yaw1 * eIp;
        return kf;
      }
    }
  }
  Q_ASSERT( false );
  return Keyframe();
}

void Qgs3DAnimationSettings::readXml( const QDomElement &elem )
{
  mEasingCurve = QEasingCurve( ( QEasingCurve::Type ) elem.attribute( "interpolation", "0" ).toInt() );

  mKeyframes.clear();

  QDomElement elemKeyframes = elem.firstChildElement( "keyframes" );
  QDomElement elemKeyframe = elemKeyframes.firstChildElement( "keyframe" );
  while ( !elemKeyframe.isNull() )
  {
    Keyframe kf;
    kf.time = elemKeyframe.attribute( "time" ).toFloat();
    kf.point.set( elemKeyframe.attribute( "x" ).toDouble(),
                  elemKeyframe.attribute( "y" ).toDouble(),
                  elemKeyframe.attribute( "z" ).toDouble() );
    kf.dist = elemKeyframe.attribute( "dist" ).toFloat();
    kf.pitch = elemKeyframe.attribute( "pitch" ).toFloat();
    kf.yaw = elemKeyframe.attribute( "yaw" ).toFloat();
    mKeyframes.append( kf );
    elemKeyframe = elemKeyframe.nextSiblingElement( "keyframe" );
  }
}

QDomElement Qgs3DAnimationSettings::writeXml( QDomDocument &doc ) const
{
  QDomElement elem = doc.createElement( "animation3d" );
  elem.setAttribute( "interpolation", mEasingCurve.type() );

  QDomElement elemKeyframes = doc.createElement( "keyframes" );

  for ( const Keyframe &keyframe : mKeyframes )
  {
    QDomElement elemKeyframe = doc.createElement( "keyframe" );
    elemKeyframe.setAttribute( "time", keyframe.time );
    elemKeyframe.setAttribute( "x", keyframe.point.x() );
    elemKeyframe.setAttribute( "y", keyframe.point.y() );
    elemKeyframe.setAttribute( "z", keyframe.point.z() );
    elemKeyframe.setAttribute( "dist", keyframe.dist );
    elemKeyframe.setAttribute( "pitch", keyframe.pitch );
    elemKeyframe.setAttribute( "yaw", keyframe.yaw );
    elemKeyframes.appendChild( elemKeyframe );
  }

  elem.appendChild( elemKeyframes );

  return elem;
}
