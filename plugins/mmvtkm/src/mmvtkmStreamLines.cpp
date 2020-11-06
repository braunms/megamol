/*
 * mmvtkmStreamLines.cpp
 *
 * Copyright (C) 2020 by VISUS (Universitaet Stuttgart)
 * Alle Rechte vorbehalten.
 */

#include "vtkm/filter/Streamline.h"
#include "vtkm/io/writer/VTKDataSetWriter.h"

#include "mmvtkm/mmvtkmDataCall.h"
#include "mmvtkm/mmvtkmStreamLines.h"

#include "mmcore/param/ButtonParam.h"
#include "mmcore/param/EnumParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/StringParam.h"
#include "mmcore/param/Vector3fParam.h"

#include "glm/gtx/transform.hpp"

using namespace megamol;
using namespace megamol::mmvtkm;


/**
 * mmvtkmStreamLines::mmvtkmStreamLines
 */
mmvtkmStreamLines::mmvtkmStreamLines()
    : core::Module()
    , meshCalleeSlot_("meshCalleeSlot", "Requests streamline mesh data from vtk data")
    , vtkCallerSlot_("vtkCallerSlot", "Requests vtk data for streamlines")
    , psStreamlineFieldName_("fieldName", "Specifies the field name of the streamline vector field")
    , psNumStreamlineSeeds_("numSeeds", "Specifies the number of seeds for the streamlines")
    , psStreamlineStepSize_("stepSize", "Specifies the step size for the streamlines")
    , psNumStreamlineSteps_("numSteps", "Specifies the number of steps for the streamlines")
    , psLowerStreamlineSeedBound_("lowerSeedBound", "Specifies the lower streamline seed bound")
    , psUpperStreamlineSeedBound_("upperSeedBound", "Specifies the upper streamline seed bound")
    , psSeedPlaneMode_("planeMode", "Specifies the representation of the seed plane")
    , psPlaneOrigin_("origin", "Specifies the origin of the seed plane")
    , psPlaneConnectionPoint1_(
          "Connection1", "Specifies the first point with which the origin of the seed plane is connected")
    , psPlaneConnectionPoint2_(
          "Connection2", "Specifies the second point with which the origin of the seed plane is connected")
    , psSeedPlaneNormal_("planeNormal", "Specifies the normal of the seed plane")
    , psSeedPlanePoint_("planePoint", "Specifies a point on the seed plane")
    , psSeedPlaneU_("u", "Specifies the scale in u direction")
    , psSeedPlaneV_("v", "Specifies the scale in v direction")
    //, seedPlaneDistance_("planeDistance", "Specifies the distance of the seed plane to the origin")
    , psSeedPlaneColor_("planeColor", "Specifies the color of the seed plane")
    , psSeedPlaneAlpha_("planeAlpha", "Specifies the transparency of the seed plane")
    , psApplyChanges_("apply", "Press to apply changes for streamline configuration")
    , psResampleSeeds_(
          "re-sample", "Press to re-sample the seeds of the streamlines. This deletes current streamlines.")
    , psClearGhostPlane_("clearGhostPlane", "Press to clear/hide the ghost plane")
    , streamlineUpdate_(true)
    , planeUpdate_(false)
    , planeAppearanceUpdate_(false)
    , planeNormalDirtyFlag_(false)
    , newVersion_(1)
    , streamlineOutput_()
    , dataSetBounds_()
    , maxBoundLength_(0.f)
    , streamlineData_{}
    , streamlineColor_{}
    , streamlineIndices_{}
    , numSeeds_(100)
    , numSteps_(2000)
    , stepSize_(0.1f)
    , activeField_("hs1")
    , planeOrigin_(0.f, 0.f, 0.f)
    , planeConnectionPoint1_(-50.f, -50.f, 0)
    , planeConnectionPoint2_(50.f, 50.f, 100.f)
    , seedPlaneNormal_(1.f, 0.f, 0.f)
    , seedPlanePoint_(0.f, 0.f, 50.f)
    , seedPlaneDistance_(0.f)
    , seedPlaneColor_(0.5f, 0.f, 0.f)
    , seedPlaneAlpha_(1.f)
    , liveSeedPlane_{}
    , originalSeedPlane_{}
    , uSeedPlane_{}
    , vSeedPlane_{}
    , seedPlaneColorVec_{}
    , seedPlaneIndices_{}
    , seedPlaneTriangles_{}
    , seeds_{}
    , planeMode_(0)
	, ghostCopy_{} {
    this->meshCalleeSlot_.SetCallback(mesh::CallMesh::ClassName(),
        mesh::CallMesh::FunctionName(0), // used to be mesh::CallMesh
        &mmvtkmStreamLines::getDataCallback);
    this->meshCalleeSlot_.SetCallback(
        mesh::CallMesh::ClassName(), mesh::CallMesh::FunctionName(1), &mmvtkmStreamLines::getMetaDataCallback);
    this->MakeSlotAvailable(&this->meshCalleeSlot_);

    this->vtkCallerSlot_.SetCompatibleCall<mmvtkmDataCallDescription>();
    this->MakeSlotAvailable(&this->vtkCallerSlot_);

    // TODO: instead of hardcoding fieldnames,
    // maybe also read all field names and show them as dropdown menu in megamol
    this->psStreamlineFieldName_.SetParameter(new core::param::StringParam(activeField_));
    this->MakeSlotAvailable(&this->psStreamlineFieldName_);

    this->psNumStreamlineSeeds_.SetParameter(new core::param::IntParam(numSeeds_, 0));
    this->MakeSlotAvailable(&this->psNumStreamlineSeeds_);

    this->psStreamlineStepSize_.SetParameter(new core::param::FloatParam(stepSize_, 0.f));
    this->MakeSlotAvailable(&this->psStreamlineStepSize_);
    this->psStreamlineStepSize_.Parameter()->SetGUIPresentation(
        core::param::AbstractParamPresentation::Presentation::Knob);

    this->psNumStreamlineSteps_.SetParameter(new core::param::IntParam(numSteps_, 1));
    this->MakeSlotAvailable(&this->psNumStreamlineSteps_);

    this->psLowerStreamlineSeedBound_.SetParameter(new core::param::Vector3fParam({0, 0, 0}));
    this->psLowerStreamlineSeedBound_.Param<core::param::Vector3fParam>()->SetGUIReadOnly(true);
    this->MakeSlotAvailable(&this->psLowerStreamlineSeedBound_);

    this->psUpperStreamlineSeedBound_.SetParameter(new core::param::Vector3fParam({1, 1, 1}));
    this->psUpperStreamlineSeedBound_.Param<core::param::Vector3fParam>()->SetGUIReadOnly(true);
    this->MakeSlotAvailable(&this->psUpperStreamlineSeedBound_);

    this->psSeedPlaneMode_.SetParameter(new core::param::EnumParam(planeMode_));
    this->psSeedPlaneMode_.Param<core::param::EnumParam>()->SetTypePair(0, "normal");
    this->psSeedPlaneMode_.Param<core::param::EnumParam>()->SetTypePair(1, "parameter");
    this->psSeedPlaneMode_.SetUpdateCallback(&mmvtkmStreamLines::planeModeChanged);
    this->MakeSlotAvailable(&this->psSeedPlaneMode_);

    this->psPlaneOrigin_.SetParameter(new core::param::Vector3fParam({0.f, 0.f, 0.f}));
    this->MakeSlotAvailable(&this->psPlaneOrigin_);
    this->psPlaneOrigin_.Param<core::param::Vector3fParam>()->SetGUIVisible(false);

    this->psPlaneConnectionPoint1_.SetParameter(new core::param::Vector3fParam({-50.f, -50.f, 0.f}));
    this->MakeSlotAvailable(&this->psPlaneConnectionPoint1_);
    this->psPlaneConnectionPoint1_.Param<core::param::Vector3fParam>()->SetGUIVisible(false);

    this->psPlaneConnectionPoint2_.SetParameter(new core::param::Vector3fParam({50.f, 50.f, 100.f}));
    this->MakeSlotAvailable(&this->psPlaneConnectionPoint2_);
    this->psPlaneConnectionPoint2_.Param<core::param::Vector3fParam>()->SetGUIVisible(false);

    this->psSeedPlaneNormal_.SetParameter(new core::param::Vector3fParam({1.f, 0.f, 0.f}));
    this->psSeedPlaneNormal_.SetUpdateCallback(&mmvtkmStreamLines::ghostPlane);
    this->MakeSlotAvailable(&this->psSeedPlaneNormal_);
    this->psSeedPlaneNormal_.Parameter()->SetGUIPresentation(
        core::param::AbstractParamPresentation::Presentation::Rotation3D_Direction);

    this->psSeedPlanePoint_.SetParameter(new core::param::Vector3fParam({0.f, 0.f, 50.f}));
    this->MakeSlotAvailable(&this->psSeedPlanePoint_);

    this->psSeedPlaneU_.SetParameter(new core::param::FloatParam(0.2f, 0.f, 1.f));
    this->psSeedPlaneU_.SetUpdateCallback(&mmvtkmStreamLines::assignUV);
    this->MakeSlotAvailable(&this->psSeedPlaneU_);
    this->psSeedPlaneU_.Parameter()->SetGUIPresentation(core::param::AbstractParamPresentation::Presentation::Knob);

    this->psSeedPlaneV_.SetParameter(new core::param::FloatParam(0.f, 0.f, 1.f));
    this->psSeedPlaneV_.SetUpdateCallback(&mmvtkmStreamLines::assignUV);
    this->MakeSlotAvailable(&this->psSeedPlaneV_);
    this->psSeedPlaneV_.Parameter()->SetGUIPresentation(core::param::AbstractParamPresentation::Presentation::Knob);

    // this->psSeedPlaneDistance_.SetParameter(new core::param::FloatParam(psSeedPlaneDistance_));
    // this->MakeSlotAvailable(&this->psSeedPlaneDistance_);

    this->psSeedPlaneColor_.SetParameter(
        new core::param::Vector3fParam({red_.x, red_.y, red_.z}, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}));
    this->MakeSlotAvailable(&this->psSeedPlaneColor_);

    this->psSeedPlaneAlpha_.SetParameter(new core::param::FloatParam(seedPlaneAlpha_, 0.f, 1.f));
    this->MakeSlotAvailable(&this->psSeedPlaneAlpha_);

    this->psApplyChanges_.SetParameter(new core::param::ButtonParam());
    this->psApplyChanges_.SetUpdateCallback(&mmvtkmStreamLines::applyChanges);
    this->MakeSlotAvailable(&this->psApplyChanges_);

    this->psResampleSeeds_.SetParameter(new core::param::ButtonParam());
    this->psResampleSeeds_.SetUpdateCallback(&mmvtkmStreamLines::setResampleSeeds);
    this->MakeSlotAvailable(&this->psResampleSeeds_);

    this->psClearGhostPlane_.SetParameter(new core::param::ButtonParam());
    this->psClearGhostPlane_.SetUpdateCallback(&mmvtkmStreamLines::clearGhostPlaneCallBack);
    this->MakeSlotAvailable(&this->psClearGhostPlane_);
}


/**
 * mmvtkmStreamLines::~mmvtkmStreamLines
 */
mmvtkmStreamLines::~mmvtkmStreamLines() { this->Release(); }


/**
 * mmvtkmStreamLines::release
 */
void mmvtkmStreamLines::release() {}


/**
 * mmvtkmStreamLines::create
 */
bool mmvtkmStreamLines::create() {
    this->meshDataAccess_ = std::make_shared<mesh::MeshDataAccessCollection>();
    return true;
}


/**
 * mmvtkmStreamLines::assignUV
 */
bool mmvtkmStreamLines::assignUV(core::param::ParamSlot& slot) {
    float u = this->psSeedPlaneU_.Param<core::param::FloatParam>()->Value();
    float v = this->psSeedPlaneV_.Param<core::param::FloatParam>()->Value();

    // TODO: if seedplane is not aligned with ghostplane, then u and v shouldnt have an effect?
    // i.e. only effect after pressing apply and seedplanenormal is not dirty

	
    // intersect line with seedplane
    calcLiveSeedPlane(true, u, ghostCopy_[0], ghostCopy_[2], originalSeedPlane_);
    calcLiveSeedPlane(false, v, ghostCopy_[1], ghostCopy_[3], uSeedPlane_);


    // adapt colorvec and indexvec according to new plane
    unsigned int num = liveSeedPlane_.size();
    seedPlaneColorVec_.clear();
    seedPlaneIndices_.clear();

    for (int i = 0; i < num; ++i) {
        seedPlaneColorVec_.push_back(glm::vec4(glm::vec3(i * 1.f / (num - 1)), seedPlaneAlpha_));
        seedPlaneIndices_.push_back(i);
	}
    
	// avoids complicated insert/erase handling of vector
	mesh::MeshDataAccessCollection::Mesh ghostMda = meshDataAccess_->accessMesh().back();
    meshDataAccess_->accessMesh().pop_back();	// pop ghostplane
    meshDataAccess_->accessMesh().pop_back();	// pop liveseedplane
    createAndAddMeshDataToCall(liveSeedPlane_, seedPlaneColorVec_, seedPlaneIndices_, liveSeedPlane_.size(),
        seedPlaneIndices_.size(), mesh::MeshDataAccessCollection::PrimitiveType::TRIANGLE_FAN);
    meshDataAccess_->accessMesh().push_back(ghostMda);

	
	planeAppearanceUpdate_ = true;


    return true;
}


/**
 * mmvtkmStreamLines::calcLiveSeedPlane
 */
void mmvtkmStreamLines::calcLiveSeedPlane(
    bool isU, float uv, const glm::vec3& start, const glm::vec3& stop, const std::vector<glm::vec3>& plane) {
    glm::vec3 d = stop - start;
    float l = glm::length(d);
    glm::vec3 dir = glm::normalize(d);
    // orthogonal line to v02 that also lies in plane
    glm::vec3 vBorder = glm::normalize(glm::cross(d, seedPlaneNormal_));


    float length = uv * l;
    glm::vec3 pBorder = start + length * dir;
    borderLine_[isU ? 0 : 2] = pBorder - maxBoundLength_ * vBorder;
    borderLine_[isU ? 1 : 3] = pBorder + maxBoundLength_ * vBorder;

    unsigned int numSeedPlanePoints = plane.size();

    // check all dot products first to check whether the whole plane
    // is in front of or behind the line
    unsigned int cnt = 0;
    std::vector<float> dots;
    for (int i = 0; i < numSeedPlanePoints; ++i) {
        glm::vec3 p = plane[i];
        float dot = glm::dot(dir, p - pBorder);
        dots.push_back(dot);
        if (dot > 0.f) {
            cnt++;
        }
    }


    if (0 < cnt && cnt < numSeedPlanePoints) {
        liveSeedPlane_.clear();

        for (int i = 0; i < numSeedPlanePoints; ++i) {
            glm::vec3 p = plane[i];

            if (dots[i] >= 0.f) {
                // acute angle --> same direction --> p is in front of line
                // no need to do anything at this point
                liveSeedPlane_.push_back(p);
            } else {
                // obtuse angle --> p is behind the line
				// check if predecessor and successor are behind or in front of border
                unsigned int preId = i == 0 ? numSeedPlanePoints - 1 : i - 1;
                unsigned int sucId = i == numSeedPlanePoints - 1 ? 0 : i + 1;
                glm::vec3 pPredecessor = plane[preId];
                glm::vec3 pSuccessor = plane[sucId];


                // first add ip of pPredecessor then of pSuccessor side
                glm::vec3 ip(0.f);
                if (dots[preId] > 0.f) {
					if (lineLineIntersection(pPredecessor, p, pBorder, vBorder, ip)) {
					    liveSeedPlane_.push_back(ip);
					}
				}

				glm::vec3 ip2(0.f);
				if (dots[sucId] > 0.f) {
					if (lineLineIntersection(pSuccessor, p, pBorder, vBorder, ip2)) {
					    liveSeedPlane_.push_back(ip2);
					}
				}
            }
        }
    } else if(cnt == numSeedPlanePoints) {
        liveSeedPlane_ = plane;
    } else {
        liveSeedPlane_.clear();
        liveSeedPlane_ = {glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 0.f)};
	}

	if (isU) {
        uSeedPlane_.clear();
        uSeedPlane_ = liveSeedPlane_;
    }
}


/**
 * mmvtkmStreamLines::lineLineIntersection
 */
bool mmvtkmStreamLines::lineLineIntersection(
    const glm::vec3& p1, const glm::vec3& p, const glm::vec3& pBorder, const glm::vec3& vBorder, glm::vec3& ip) {

    // intersection test for line-line-intersection taken from:
    // http://mathforum.org/library/drmath/view/62814.html
    glm::vec3 vl1 = glm::normalize(p1 - p);
    glm::vec3 vl2 = glm::normalize(vBorder);

    glm::vec3 vCrossLhs = glm::cross(vl1, vl2);

    glm::vec3 pCrossRhs = glm::cross(pBorder - p, vl2);

    // both conditions have to be met in order for a intersection to occur
    if (!isNullVector(vCrossLhs) && areParallel(vCrossLhs, pCrossRhs)) {
        float t = glm::length(pCrossRhs) / glm::length(vCrossLhs);

        // check if directions are the same
        if (glm::dot(pCrossRhs, vCrossLhs) < 0.f) {
            t = -t;
        }

        glm::vec3 i = p + t * vl1;

        // check if intersection point is between the two points p and p1
        if (isBetween(i, p, p1)) {

            ip = i;
            // check if new ip is the same as an already included point in the plane
            // avoids redundancies
            return true;
        }
    }

    return false;
}


/**
 * mmvtkmStreamLines::ghostPlane
 */
bool mmvtkmStreamLines::ghostPlane(core::param::ParamSlot& slot) {
    planeNormalDirtyFlag_ = true;

    visVec3f normal = slot.Param<core::param::Vector3fParam>()->Value();
    glm::vec3 spn = glm::normalize(glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ()));

    glm::vec3 pp = seedPlanePoint_;

    float scale = sqrt(3.f);
    float lx = dataSetBounds_.X.Length();
    float ly = dataSetBounds_.Y.Length();
    float lz = dataSetBounds_.Z.Length();
    maxBoundLength_ = std::max(lx, std::max(ly, lz)) * scale;

    glm::vec3 vUp(0.f, 0.f, 0.f);
    vUp.x = 0.f;
    vUp.y = 1.f;
    vUp.z = (-vUp.x * spn.x - vUp.y * spn.y) / spn.z;
    vUp = glm::normalize(vUp);

    float x0 = pp.x - lx;
    float z0 = pp.z - lz;
    float y0 = (-spn.x * (x0 - pp.x) - spn.z * (z0 - pp.z)) / spn.y + pp.y;
    glm::vec3 p = glm::vec3(x0, y0, z0);
    glm::vec3 v0 = glm::normalize(p - pp);

    glm::vec3 p0 = pp + v0 * maxBoundLength_;
    glm::vec3 p1 = glm::rotate(3.14159265358979f / 2.f, spn) * glm::vec4(p0 - pp, 1.f);
    glm::vec3 v1 = glm::normalize(p1);
    p1 = pp + v1 * maxBoundLength_;
    glm::vec3 p2 = pp - v0 * maxBoundLength_;
    glm::vec3 p3 = pp - v1 * maxBoundLength_;


	// CAUTION: EXTREMELY UNSAFE!
	// this only works because the ghostplane always consists of 4 vertices
	// if this varies, the meshdataaccess_ has to be modified directly
    glm::vec3 zFightingOffset = 0.1f * spn;	// prevents z-fighting of ghost- and liveseedplane
	ghostPlane_.clear();
    ghostPlane_ = {p0 + zFightingOffset, p1 + zFightingOffset, p2 + zFightingOffset, p3 + zFightingOffset};
    ghostCopy_.clear();
    ghostCopy_ = {p0, p1, p2, p3};
	

    planeAppearanceUpdate_ = true;

    return true;
}


/**
 * mmvtkmStreamLines::applyChanges
 */
bool mmvtkmStreamLines::applyChanges(core::param::ParamSlot& slot) {
    bool fieldName = this->psStreamlineFieldName_.IsDirty();
    bool numSeeds = this->psNumStreamlineSeeds_.IsDirty();
    bool stepSize = this->psStreamlineStepSize_.IsDirty();
    bool numSteps = this->psNumStreamlineSteps_.IsDirty();
    bool streamlineSeeds = fieldName || numSeeds || stepSize || numSteps;


    bool origin = this->psPlaneOrigin_.IsDirty();
    bool connect1 = this->psPlaneConnectionPoint1_.IsDirty();
    bool connect2 = this->psPlaneConnectionPoint2_.IsDirty();
    bool planeNormal = this->psSeedPlaneNormal_.IsDirty() || planeNormalDirtyFlag_;
    bool planePoint = this->psSeedPlanePoint_.IsDirty();
    bool plane = origin || connect1 || connect2 || planeNormal || planePoint;


    bool planeColor = this->psSeedPlaneColor_.IsDirty();
    bool planeAlpha = this->psSeedPlaneAlpha_.IsDirty();
    bool appearance = planeColor || planeAlpha;


    // TODO: check if order is important:
    // e.g. if streamlineSeeds is called, there is no need to call appearance beforehand
    // because streamlineSeeds includes appearance
    if (appearance) {
        if (!setPlaneAndAppearanceUpdate()) return false;
    }

    if (plane) {
        if (!setPlaneUpdate()) return false;
    }

    if (streamlineSeeds) {
        if (!setStreamlineAndResampleSeedsUpdate()) return false;
    }


    this->psStreamlineFieldName_.ResetDirty();
    this->psNumStreamlineSeeds_.ResetDirty();
    this->psStreamlineStepSize_.ResetDirty();
    this->psNumStreamlineSteps_.ResetDirty();


    this->psPlaneOrigin_.ResetDirty();
    this->psPlaneConnectionPoint1_.ResetDirty();
    this->psPlaneConnectionPoint2_.ResetDirty();
    this->psSeedPlaneNormal_.ResetDirty();
    planeNormalDirtyFlag_ = false;
    this->psSeedPlanePoint_.ResetDirty();


    this->psSeedPlaneColor_.ResetDirty();
    this->psSeedPlaneAlpha_.ResetDirty();


	psSeedPlaneU_.Param<core::param::FloatParam>()->SetValue(0.f);
	psSeedPlaneV_.Param<core::param::FloatParam>()->SetValue(0.f);


    return true;
}


/**
 * mmvtkmStreamLines::planeModeChanged
 */
bool mmvtkmStreamLines::planeModeChanged(core::param::ParamSlot& slot) {
    if (slot.Param<core::param::EnumParam>()->Value() == NORMAL) {
        planeMode_ = 0;
        this->psPlaneOrigin_.Param<core::param::Vector3fParam>()->SetGUIVisible(false);
        this->psPlaneConnectionPoint1_.Param<core::param::Vector3fParam>()->SetGUIVisible(false);
        this->psPlaneConnectionPoint2_.Param<core::param::Vector3fParam>()->SetGUIVisible(false);
        this->psSeedPlaneNormal_.Param<core::param::Vector3fParam>()->SetGUIVisible(true);
        this->psSeedPlanePoint_.Param<core::param::Vector3fParam>()->SetGUIVisible(true);
        this->psSeedPlaneU_.Param<core::param::FloatParam>()->SetGUIVisible(true);
        this->psSeedPlaneV_.Param<core::param::FloatParam>()->SetGUIVisible(true);
        // this->psSeedPlaneDistance_.Param<core::param::FloatParam>()->SetGUIVisible(true);
    } else if (slot.Param<core::param::EnumParam>()->Value() == PARAMETER) {
        planeMode_ = 1;
        this->psPlaneOrigin_.Param<core::param::Vector3fParam>()->SetGUIVisible(true);
        this->psPlaneConnectionPoint1_.Param<core::param::Vector3fParam>()->SetGUIVisible(true);
        this->psPlaneConnectionPoint2_.Param<core::param::Vector3fParam>()->SetGUIVisible(true);
        this->psSeedPlaneNormal_.Param<core::param::Vector3fParam>()->SetGUIVisible(false);
        this->psSeedPlanePoint_.Param<core::param::Vector3fParam>()->SetGUIVisible(false);
        this->psSeedPlaneU_.Param<core::param::FloatParam>()->SetGUIVisible(false);
        this->psSeedPlaneV_.Param<core::param::FloatParam>()->SetGUIVisible(false);
        // this->psSeedPlaneDistance_.Param<core::param::FloatParam>()->SetGUIVisible(false);

        ghostPlane_.clear();
        ghostPlane_ = {glm::vec3(0.f), glm::vec3(0.f), glm::vec3(0.f), glm::vec3(0.f)};

        planeAppearanceUpdate_ = true;
    }


    return true;
}


/**
 * mmvtkmStreamLines::setResampleSeeds
 */
bool mmvtkmStreamLines::setResampleSeeds(core::param::ParamSlot& slot) {
    streamlineUpdate_ = true;
    return true;
}


/**
 * mmvtkmStreamLines::clearGhostPlane
 */
void mmvtkmStreamLines::clearGhostPlane() {
    ghostPlane_.clear();
    ghostPlane_ = {glm::vec3(0.f), glm::vec3(0.f), glm::vec3(0.f), glm::vec3(0.f)};
}


/**
 * mmvtkmStreamLines::clearGhostPlane
 */
bool mmvtkmStreamLines::clearGhostPlaneCallBack(core::param::ParamSlot& slot) {
    clearGhostPlane();

    planeAppearanceUpdate_ = true;

    return true;
}


/**
 * mmvtkmStreamLines::setStreamlineAndResampleSeedsUpdate
 */
bool mmvtkmStreamLines::setStreamlineAndResampleSeedsUpdate() {
    activeField_ = this->psStreamlineFieldName_.Param<core::param::StringParam>()->Value();
    numSeeds_ = this->psNumStreamlineSeeds_.Param<core::param::IntParam>()->Value();
    stepSize_ = this->psStreamlineStepSize_.Param<core::param::FloatParam>()->Value();
    numSteps_ = this->psNumStreamlineSteps_.Param<core::param::IntParam>()->Value();


    streamlineUpdate_ = true;


    return true;
}


/**
 * mmvtkmStreamLInes::setPlaneAndAppearanceUpdate
 *
 * This function assumes that the plane is at the very last position in the mesh
 */
bool mmvtkmStreamLines::setPlaneAndAppearanceUpdate() {
    visVec3f tmpColor = psSeedPlaneColor_.Param<core::param::Vector3fParam>()->Value();
    seedPlaneColor_ = glm::vec3(tmpColor.GetX(), tmpColor.GetY(), tmpColor.GetZ());
    seedPlaneAlpha_ = psSeedPlaneAlpha_.Param<core::param::FloatParam>()->Value();


    int numColors = seedPlaneColorVec_.size();
    seedPlaneColorVec_.clear();

    for (int i = 0; i < numColors; ++i) {
        seedPlaneColorVec_.push_back(glm::vec4(seedPlaneColor_, seedPlaneAlpha_));
    }


    planeAppearanceUpdate_ = true;

    return true;
}


/**
 * mmvtkmStreamLines::setPlaneUpdate
 */
bool mmvtkmStreamLines::setPlaneUpdate() {
    visVec3f origin = this->psPlaneOrigin_.Param<core::param::Vector3fParam>()->Value();
    planeOrigin_ = {origin.GetX(), origin.GetY(), origin.GetZ()};
    visVec3f ll = this->psPlaneConnectionPoint1_.Param<core::param::Vector3fParam>()->Value();
    planeConnectionPoint1_ = {ll.GetX(), ll.GetY(), ll.GetZ()};
    visVec3f ur = this->psPlaneConnectionPoint2_.Param<core::param::Vector3fParam>()->Value();
    planeConnectionPoint2_ = {ur.GetX(), ur.GetY(), ur.GetZ()};
    visVec3f normal = this->psSeedPlaneNormal_.Param<core::param::Vector3fParam>()->Value();
    seedPlaneNormal_ = {normal.GetX(), normal.GetY(), normal.GetZ()};
    visVec3f point = this->psSeedPlanePoint_.Param<core::param::Vector3fParam>()->Value();
    seedPlanePoint_ = {point.GetX(), point.GetY(), point.GetZ()};
    // psSeedPlaneDistance_ = this->seedPlaneDistance_.Param<core::param::FloatParam>()->Value();


    if (isNullVector(normal)) {
        core::utility::log::Log::DefaultLog.WriteError("Plane normal is null vector.");

        return false;
    }


    planeUpdate_ = true;
    streamlineUpdate_ = true;


    return true;
}


/**
 * mmvtkmStreamLines::calcPlaneBboxIntersectionPoints
 */
std::vector<glm::vec3> mmvtkmStreamLines::calcPlaneBboxIntersectionPoints(
    const visPlanef& samplePlane, const vtkm::Bounds& bounds) {
    std::vector<glm::vec3> intersectionPoints;
    seedPlaneColorVec_.clear();
    seedPlaneIndices_.clear();

    vislib::math::Cuboid<float> bbox(
        bounds.X.Min, bounds.Y.Min, bounds.Z.Min, bounds.X.Max, bounds.Y.Max, bounds.Z.Max);


    // order of returned point pairs:
    // front left - front top - front right - front bottom
    // back left - back top - back right - back bottom
    // mid bottom left - mid bottom right - mid top left - mid top right
    std::vector<vislib::Pair<visPoint3f, visPoint3f>> itOut(12);
    bbox.GetLineSegments(itOut.data());


    // intersect every line against the sample plane
    int cnt = 0;
    for (const auto& line : itOut) {
        visPoint3f ip;
        int numIntersections = samplePlane.Intersect(ip, line.GetFirst(), line.GetSecond());

        // if line lies within the plane, then ip = line.GetFirst()
        // so no need for further checks
        if (numIntersections != 0) {

            if (isOutsideBbox(glm::vec3(ip[0], ip[1], ip[2]), bounds)) {
                continue;
            }

            intersectionPoints.push_back(glm::vec3(ip[0], ip[1], ip[2]));
            seedPlaneColorVec_.push_back(glm::vec4(seedPlaneColor_, seedPlaneAlpha_));
            seedPlaneIndices_.push_back(cnt++);
        }
    }

    orderPolygonVertices(intersectionPoints);

    return intersectionPoints;
}


/**
 * mmvtkmStreamLines::isOutsideBbox
 */
bool mmvtkmStreamLines::isOutsideBbox(const glm::vec3& p, const vtkm::Bounds& bounds) {
    bool left = p.x < bounds.X.Min;
    bool right = p.x > bounds.X.Max;
    bool x_out = left || right;

    bool bottom = p.y < bounds.Y.Min;
    bool top = p.y > bounds.Y.Max;
    bool y_out = bottom || top;

    bool back = p.z < bounds.Z.Min;
    bool front = p.z > bounds.Z.Max;
    bool z_out = back || front;

    bool is_out = x_out || y_out || z_out;


    return is_out;
}


/**
 * mmvtkmStreamLines::orderPolygonVertices
 */
void mmvtkmStreamLines::orderPolygonVertices(std::vector<glm::vec3>& vertices) {

    if (vertices.size() < 2) {
        core::utility::log::Log::DefaultLog.WriteError(
            "In %s at line %d.\nNumber of vertices is too low: %i. Needs to be at least 2.", __FILE__, __LINE__,
            vertices.size());
        return;
    }


    float numVertices = (float)vertices.size();
    std::vector<std::pair<float, glm::vec3>> angles;
    glm::vec3 center = vertices[0] + 0.5f * (vertices[1] - vertices[0]);


    visVec3f tmpNormal = psSeedPlaneNormal_.Param<core::param::Vector3fParam>()->Value();
    glm::vec3 nPlane = glm::normalize(glm::vec3(tmpNormal[0], tmpNormal[1], tmpNormal[2]));

    glm::vec3 xAxis(1.f, 0.f, 0.f);
    glm::vec3 yAxis(0.f, 1.f, 0.f);
    glm::vec3 zAxis(0.f, 0.f, 1.f);


    // construct vector for calculating angle between x- and y-axis
    glm::vec3 projectedXYDir;
    if (isZerof(nPlane.x) && isZerof(nPlane.y)) {
        projectedXYDir = glm::normalize(glm::vec3(nPlane.x, nPlane.y, 1.f));
    } else {
        projectedXYDir = glm::normalize(glm::vec3(nPlane.x, nPlane.y, 0.f));
    }
    float angleAroundZ = acos(glm::dot(projectedXYDir, yAxis));
    // if x points in neg dir, then rotation needs to occur to the right
    if (nPlane.x < 0.f) angleAroundZ = -angleAroundZ;
    glm::mat4 rotateAroundZ = glm::rotate(glm::mat4(1.f), angleAroundZ, zAxis);
    glm::vec3 rotatedNormalAroundZ = rotateAroundZ * glm::vec4(nPlane, 1.f);

    // construct vector for calculating angle between y- and z-axis
    glm::vec3 projectedYZDir = glm::normalize(glm::vec3(0.f, rotatedNormalAroundZ.y, rotatedNormalAroundZ.z));
    float angleAroundX = acos(glm::dot(projectedYZDir, yAxis));
    // if z points in pos dir, then rotation needs to occur to the right
    if (rotatedNormalAroundZ.z > 0.f) angleAroundX = -angleAroundX;
    glm::mat4 rotateAroundX = glm::rotate(glm::mat4(1.f), angleAroundX, xAxis);

    glm::mat4 rotToXZ = rotateAroundX * rotateAroundZ;
    // glm::mat4 invRotToXZ = glm::inverse(rotToXZ);


    // calc angles and sort by this angle (ascending order)
    for (const auto& v : vertices) {
        // transform current vertex into xz-plane
        glm::vec3 rotatedV = rotToXZ * glm::vec4(v, 1.f);
        glm::vec3 rotatedCenter = rotToXZ * glm::vec4(center, 1.f);
        glm::vec3 edgePoint = glm::vec4(rotatedV.x, rotatedCenter.y, rotatedCenter.z, 1.f);
        glm::vec3 opp = rotatedV - edgePoint;
        glm::vec3 adj = edgePoint - rotatedCenter;

        float oppLength = opp.z > 0.f ? glm::length(opp) : -glm::length(opp);
        float adjLength = adj.x > 0.f ? glm::length(adj) : -glm::length(adj);

        // can be done better without using atan2f
        // instead just compare x and z values
        float angle = atan2f(oppLength, adjLength);

        angles.push_back(std::pair<float, glm::vec3>(angle, v));
    }


    std::sort(angles.begin(), angles.end(),
        [](const std::pair<float, glm::vec3>& a, const std::pair<float, glm::vec3>& b) -> bool {
            return a.first < b.first;
        });


    for (int i = 0; i < (int)numVertices; ++i) {
        vertices[i] = angles[i].second;
    }


    // need to ensure that there are no duplicates in the list
    // other wrong triangles are created
    // this happens e. g. when the plane intersects the corners of the bbox
    std::vector<glm::vec3>::iterator it;
    it = std::unique_copy(vertices.begin(), vertices.end(), vertices.begin());
    ptrdiff_t d = std::distance(vertices.begin(), it);
    vertices.resize(d);
    seedPlaneColorVec_.resize(d);
    seedPlaneIndices_.resize(d);
}


/**
 * mmvtkmStreamLines::decomposePolygon
 *
 * This function requires the input vertices to be in a triangle fan order.
 * Otherwise the triangles might overlap and the polygon is not completely
 * represented by the triangles.
 */
std::vector<mmvtkmStreamLines::Triangle> mmvtkmStreamLines::decomposePolygon(const std::vector<glm::vec3>& polygon) {
    std::vector<Triangle> triangles;

    int numVertices = polygon.size();


    glm::vec3 fix = polygon[0];
    glm::vec3 last = polygon[1];

    float polygonArea = 0.f;

    for (int i = 2; i < numVertices; ++i) {
        glm::vec3 next = polygon[i];
        Triangle tri(fix, last, next);
        polygonArea += tri.area;

        triangles.push_back(tri);

        last = next;
    }


    // weight triangle according to its area for normal distribution sampling
    for (auto& tri : triangles) {
        tri.weight = tri.area / polygonArea;
    }


    return triangles;
}


/**
 * mmvtkmStreamLines::isInsideTriangle
 */
bool mmvtkmStreamLines::isInsideTriangle(const glm::vec3& p, const Triangle& tri) {
    // calc barycentric coordinates
    glm::vec3 v3 = p - tri.o;

    float d00 = glm::dot(tri.v1, tri.v1);
    float d01 = glm::dot(tri.v1, tri.v2);
    float d11 = glm::dot(tri.v2, tri.v2);
    float d20 = glm::dot(v3, tri.v1);
    float d21 = glm::dot(v3, tri.v2);
    float denom = d00 * d11 - d01 * d01;
    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    return v > 0.f && w > 0.f && u > 0.f;

    // alternativ approach: check if point is on correct side of the line bc (if a is origin)
    // http://alienryderflex.com/point_left_of_ray/
    // could be more efficient if transformation can be done efficiently
}


/**
 * mmvtkmStreamLines::createAndAddMeshDataToCall
 */
bool mmvtkmStreamLines::createAndAddMeshDataToCall(std::vector<glm::vec3>& data, std::vector<glm::vec4>& color,
    std::vector<unsigned int>& idcs, int numPoints, int numIndices, mesh::MeshDataAccessCollection::PrimitiveType pt) {

    if (data.size() != color.size() && data.size() != idcs.size()) {
        core::utility::log::Log::DefaultLog.WriteError("In %s at line %d.\nNumber of vertices, colors, "
                                                       "and indices are not equal.",
            __FILE__, __LINE__);
        return false;
    }

    if (data.size() < 2 || color.size() == 0 || idcs.size() == 0) {
        //core::utility::log::Log::DefaultLog.WriteError("In %s at line %d.\nLineData, color, " \
		"or index vector is empty.", __FILE__, __LINE__);
        return false;
    }


    mesh::MeshDataAccessCollection::VertexAttribute va;
    va.data = reinterpret_cast<uint8_t*>(data.data());
    va.byte_size = 3 * numPoints * sizeof(float);
    va.component_cnt = 3;
    va.component_type = mesh::MeshDataAccessCollection::ValueType::FLOAT;
    va.stride = 0;
    va.offset = 0;
    va.semantic = mesh::MeshDataAccessCollection::AttributeSemanticType::POSITION;

    mesh::MeshDataAccessCollection::VertexAttribute vColor;
    vColor.data = reinterpret_cast<uint8_t*>(color.data());
    vColor.byte_size = 4 * numPoints * sizeof(float);
    vColor.component_cnt = 4;
    vColor.component_type = mesh::MeshDataAccessCollection::ValueType::FLOAT;
    vColor.stride = 0;
    vColor.offset = 0;
    vColor.semantic = mesh::MeshDataAccessCollection::AttributeSemanticType::COLOR;

    mesh::MeshDataAccessCollection::IndexData idxData;
    idxData.data = reinterpret_cast<uint8_t*>(idcs.data());
    idxData.byte_size = numIndices * sizeof(unsigned int);
    idxData.type = mesh::MeshDataAccessCollection::ValueType::UNSIGNED_INT;


    if (!addMeshDataToCall({va, vColor}, idxData, pt)) {
        return false;
    }

    return true;
}


/**
 * mmvtkmStreamLines::addMeshDataToCall
 */
bool mmvtkmStreamLines::addMeshDataToCall(const std::vector<mesh::MeshDataAccessCollection::VertexAttribute>& va,
    const mesh::MeshDataAccessCollection::IndexData& id, mesh::MeshDataAccessCollection::PrimitiveType pt) {

    if (va.size() == 0 || id.data == nullptr) {
        core::utility::log::Log::DefaultLog.WriteError(
            "In %s at line %d.\LineData, color, or index vector is empty.", __FILE__, __LINE__);
        return false;
    }


    this->meshDataAccess_->addMesh(va, id, pt);

    return true;
}


/**
 * mmvtkmStreamLines::getDataCallback
 */
bool mmvtkmStreamLines::getDataCallback(core::Call& caller) {
    mmvtkm::mmvtkmDataCall* rhsVtkmDc = this->vtkCallerSlot_.CallAs<mmvtkm::mmvtkmDataCall>();
    if (rhsVtkmDc == nullptr) {
        core::utility::log::Log::DefaultLog.WriteError("In %s at line %d.\rhsVtkmDc is nullptr.", __FILE__, __LINE__);
        return false;
    }

    mesh::CallMesh* lhsMeshDc = dynamic_cast<mesh::CallMesh*>(&caller);
    if (lhsMeshDc == nullptr) {
        core::utility::log::Log::DefaultLog.WriteError("In %s at line %d.\nlhsMeshDc is nullptr. ", __FILE__, __LINE__);
        return false;
    }

    if (!(*rhsVtkmDc)(0)) {
        return false;
    }


    // this case only occurs when parameters of the plane, such as color or alpha, are changed
    // so we can early terminate
    if (planeAppearanceUpdate_) {
        lhsMeshDc->setData(meshDataAccess_, ++this->newVersion_);
        planeAppearanceUpdate_ = false;
        return true;
    }


    bool vtkmUpdate = rhsVtkmDc->HasUpdate();
    // plane calculation part here
    if (vtkmUpdate || planeUpdate_) {
        if (vtkmUpdate) {
            if (!(*rhsVtkmDc)(1)) {
                return false;
            }

            dataSetBounds_ = rhsVtkmDc->GetBounds();
            visVec3f low = {(float)dataSetBounds_.X.Min, (float)dataSetBounds_.Y.Min, (float)dataSetBounds_.Z.Min};
            visVec3f up = {(float)dataSetBounds_.X.Max, (float)dataSetBounds_.Y.Max, (float)dataSetBounds_.Z.Max};
            this->psLowerStreamlineSeedBound_.Param<core::param::Vector3fParam>()->SetValue(low);
            this->psUpperStreamlineSeedBound_.Param<core::param::Vector3fParam>()->SetValue(up);
        }


        if (planeMode_ == 0) {
            glm::vec3 n = glm::normalize(seedPlaneNormal_);
            glm::vec3 o = seedPlanePoint_;
            // d = seedPlaneDistance_;

            visPlanef samplePlane(visPoint3f(o.x, o.y, o.z), visVec3f(n.x, n.y, n.z));

            liveSeedPlane_ = calcPlaneBboxIntersectionPoints(samplePlane, dataSetBounds_);

        } else if (planeMode_ == 1) {
            glm::vec3 dir1 = planeConnectionPoint1_ - planeOrigin_;
            glm::vec3 dir2 = planeConnectionPoint2_ - planeOrigin_;
            glm::vec3 newPoint = planeOrigin_ + dir1 + dir2;

            // TODO: check if any point is out of bounds

            liveSeedPlane_.clear();
            liveSeedPlane_ = {planeOrigin_, planeConnectionPoint1_, newPoint, planeConnectionPoint2_};
        }

        // copy of seedplane used in mmvtkmStreamLines::assignUV to re-store plane
        // if u and/or v are reverted
        originalSeedPlane_ = liveSeedPlane_;
        uSeedPlane_ = liveSeedPlane_;
        vSeedPlane_ = liveSeedPlane_;


        planeUpdate_ = false;
    }


    if (vtkmUpdate || streamlineUpdate_) {
        // decompose polygon into triangles
        seedPlaneTriangles_.clear();
        seedPlaneTriangles_ = decomposePolygon(liveSeedPlane_);

        seeds_.clear();

        this->meshDataAccess_->accessMesh().clear();


        // sample points in triangles and combine each triangles' samples
        for (const auto& tri : seedPlaneTriangles_) {
            unsigned int numTriSeeds = (unsigned int)floor(numSeeds_ * tri.weight);

            for (int i = 0; i < numTriSeeds; ++i) {
                // minor TODO: watch out for bad randomness
                float s = (float)rand() / (float)RAND_MAX;
                float t = (float)rand() / (float)RAND_MAX;
                glm::vec3 p = tri.o + s * tri.v1 + t * tri.v2;

                if (!isInsideTriangle(p, tri)) {
                    // reject via --i;
                    // alternativ: transform into other triangle (no rejection needed)
                    --i;
                    continue;
                }

                // can't just simply push back p because seedArray needs vtkm::Vec structure
                seeds_.push_back({p[0], p[1], p[2]});
            }
        }

        vtkm::cont::ArrayHandle<vtkm::Vec<vtkm::FloatDefault, 3>> seedArray = vtkm::cont::make_ArrayHandle(seeds_);


        // streamline calculation part here

        std::string activeField = static_cast<std::string>(activeField_);

        try {
            // for non-temporal data (steady flow) it holds that streamlines = streaklines = pathlines
            // therefore we can calculate the pathlines via the streamline filter
            vtkm::filter::Streamline vtkmStreamlines;
            vtkmStreamlines.SetActiveField(activeField);
            vtkmStreamlines.SetStepSize(stepSize_);
            vtkmStreamlines.SetNumberOfSteps(numSteps_);
            vtkmStreamlines.SetSeeds(seedArray);

            core::utility::log::Log::DefaultLog.WriteInfo(
                "NumSeeds: %i. StepSize: %f. NumSteps: %i.", numSeeds_, stepSize_, numSteps_);


            // calc streamlines
            const vtkm::cont::DataSet* vtkmMesh = rhsVtkmDc->GetDataSet();
            streamlineOutput_ = vtkmStreamlines.Execute(*vtkmMesh);

            // vtkm::io::writer::VTKDataSetWriter writer("streamlines.vtk");
            // writer.WriteDataSet(streamlineOutput_);
        } catch (const std::exception& e) {
            core::utility::log::Log::DefaultLog.WriteError("In % s at line %d. \n", __FILE__, __LINE__);
            core::utility::log::Log::DefaultLog.WriteError(e.what());
            return false;
        }


        // get polylines
        const vtkm::cont::DynamicCellSet& polylineSet = streamlineOutput_.GetCellSet(0);
        const vtkm::cont::CellSet* polylineSetBase = polylineSet.GetCellSetBase();
        int numPolylines = polylineSetBase->GetNumberOfCells();

        // number of points used to create the polylines (may differ for each polyline)
        std::vector<vtkm::IdComponent> numPointsInPolyline;
        for (int i = 0; i < numPolylines; ++i) {
            numPointsInPolyline.emplace_back(polylineSetBase->GetNumberOfPointsInCell(i));
        }


        // get the indices for the points of the polylines
        std::vector<std::vector<vtkm::Id>> polylinePointIds(numPolylines);
        for (int i = 0; i < numPolylines; ++i) {

            int numPoints = numPointsInPolyline[i];
            std::vector<vtkm::Id> pointIds(numPoints);

            polylineSetBase->GetCellPointIds(i, pointIds.data());

            polylinePointIds[i] = pointIds;
        }


        // there most probably will only be one coordinate system which name isn't specifically set
        // so this should be sufficient
        const vtkm::cont::CoordinateSystem& coordData = streamlineOutput_.GetCoordinateSystem(0);
        const vtkm::cont::ArrayHandleVirtualCoordinates& coordDataVirtual =
            vtkm::cont::make_ArrayHandleVirtual(coordData.GetData());
        const vtkm::ArrayPortalRef<vtkm::Vec<vtkm::FloatDefault, 3>>& coords = coordDataVirtual.GetPortalConstControl();


        // clear and resize streamline data
        streamlineData_.clear();
        streamlineData_.resize(numPolylines);
        streamlineColor_.clear();
        streamlineColor_.resize(numPolylines);
        streamlineIndices_.clear();
        streamlineIndices_.resize(numPolylines);

        // build polylines for megamol mesh
        for (int i = 0; i < numPolylines; ++i) {
            int numPoints = numPointsInPolyline[i];

            // calc data
            streamlineData_[i].clear();
            streamlineData_[i].resize(numPoints);
            streamlineColor_[i].clear();
            streamlineColor_[i].resize(numPoints);

            for (int j = 0; j < numPoints; ++j) {
                const vtkm::Vec<vtkm::FloatDefault, 3>& crnt =
                    coords.Get(polylinePointIds[i][j]); // not valid on host for cuda
                streamlineData_[i][j] = glm::vec3(crnt[0], crnt[1], crnt[2]);

                streamlineColor_[i][j] = glm::vec4(glm::vec3((float)j / (float)numPoints * 0.9f + 0.1f), 1.f);
            }


            // calc indices
            int numLineSegments = numPoints - 1;
            int numIndices = 2 * numLineSegments;

            streamlineIndices_[i].clear();
            streamlineIndices_[i].resize(numIndices);

            for (int j = 0; j < numLineSegments; ++j) {
                int idx = 2 * j;
                streamlineIndices_[i][idx + 0] = j;
                streamlineIndices_[i][idx + 1] = j + 1;
            }

            // adds the mdacs of the streamlines to the call here
            createAndAddMeshDataToCall(streamlineData_[i], streamlineColor_[i], streamlineIndices_[i], numPoints,
                numIndices, mesh::MeshDataAccessCollection::PrimitiveType::LINE_STRIP);
        }

        // adds the dummy mdac for the u and v border lines
        createAndAddMeshDataToCall(borderLine_, borderColors_, borderIdcs_, borderLine_.size(), borderIdcs_.size(),
            mesh::MeshDataAccessCollection::PrimitiveType::LINES);

		// adds the mdac for the seed plane
        createAndAddMeshDataToCall(liveSeedPlane_, seedPlaneColorVec_, seedPlaneIndices_, liveSeedPlane_.size(),
            seedPlaneIndices_.size(), mesh::MeshDataAccessCollection::PrimitiveType::TRIANGLE_FAN);

		// adds the dummy mdac for the ghost plane
        //clearGhostPlane();
        createAndAddMeshDataToCall(ghostPlane_, ghostColors_, ghostIdcs_, ghostPlane_.size(), ghostIdcs_.size(),
            mesh::MeshDataAccessCollection::PrimitiveType::TRIANGLE_FAN);


        lhsMeshDc->setData(meshDataAccess_, ++this->newVersion_);

        streamlineUpdate_ = false;

        return true;
    }


    return true;
}


/**
 * mmvtkmStreamLines::getMetaDataCallback
 */
bool mmvtkmStreamLines::getMetaDataCallback(core::Call& caller) {
    mmvtkm::mmvtkmDataCall* rhsVtkmDc = this->vtkCallerSlot_.CallAs<mmvtkm::mmvtkmDataCall>();
    if (rhsVtkmDc == nullptr) {
        core::utility::log::Log::DefaultLog.WriteError("In %s at line %d.\nrhsVtkmDc is nullptr.", __FILE__, __LINE__);
        return false;
    }

    mesh::CallMesh* lhsMeshDc = dynamic_cast<mesh::CallMesh*>(&caller);
    if (lhsMeshDc == nullptr) {
        core::utility::log::Log::DefaultLog.WriteError("In %s at line %d.\nlhsMeshDc is nullptr.", __FILE__, __LINE__);
        return false;
    }

    if (!(*rhsVtkmDc)(1)) {
        return false;
    }

    if (rhsVtkmDc->HasUpdate()) {
        std::array<float, 6> bbox;
        bbox[0] = dataSetBounds_.X.Min;
        bbox[1] = dataSetBounds_.Y.Min;
        bbox[2] = dataSetBounds_.Z.Min;
        bbox[3] = dataSetBounds_.X.Max;
        bbox[4] = dataSetBounds_.Y.Max;
        bbox[5] = dataSetBounds_.Z.Max;

        auto md = lhsMeshDc->getMetaData();
        md.m_bboxs.SetBoundingBox(bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5]);
        md.m_bboxs.SetClipBox(bbox[0], bbox[1], bbox[2], bbox[3], bbox[4], bbox[5]);

        md.m_frame_cnt = 1;
        lhsMeshDc->setMetaData(md);
    }


    return true;
}