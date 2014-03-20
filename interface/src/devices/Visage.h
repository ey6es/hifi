//
//  Visage.h
//  interface
//
//  Created by Andrzej Kapolka on 2/11/14.
//  Copyright (c) 2014 High Fidelity, Inc. All rights reserved.
//

#ifndef __interface__Visage__
#define __interface__Visage__

#include <QMultiHash>
#include <QPair>
#include <QVector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VisageSDK {
    class VisageTracker2;
    struct FaceData;
}

/// Handles input from the Visage webcam feature tracking software.
class Visage : public QObject {
    Q_OBJECT
    
public:
    
    Visage();
    virtual ~Visage();
    
    void init();
    
    bool isActive() const { return _active; }
    
    const glm::quat& getHeadRotation() const { return _headRotation; }
    const glm::vec3& getHeadTranslation() const { return _headTranslation; }
    
    float getEstimatedEyePitch() const { return _estimatedEyePitch; }
    float getEstimatedEyeYaw() const { return _estimatedEyeYaw; }
    
    const QVector<float>& getBlendshapeCoefficients() const { return _blendshapeCoefficients; }
    
    void update();
    void reset();

public slots:

    void updateEnabled();
    
private:

#ifdef HAVE_VISAGE
    VisageSDK::VisageTracker2* _tracker;
    VisageSDK::FaceData* _data;
    QMultiHash<int, QPair<int, float> > _actionUnitIndexMap; 
#endif
    
    void setEnabled(bool enabled);
    
    bool _enabled;
    bool _active;
    glm::quat _headRotation;
    glm::vec3 _headTranslation;
    
    glm::vec3 _headOrigin;
    
    float _estimatedEyePitch;
    float _estimatedEyeYaw;
    
    QVector<float> _blendshapeCoefficients;
};

#endif /* defined(__interface__Visage__) */
