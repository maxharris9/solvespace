//-----------------------------------------------------------------------------
// User-initiated (not parametric) operations to modify our sketch, by
// changing the requests, like to round a corner or split curves where they
// intersect.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

//-----------------------------------------------------------------------------
// Replace constraints on oldpt with the same constraints on newpt.
// Useful when splitting, tangent arcing, or removing bezier points.
//-----------------------------------------------------------------------------
void GraphicsWindow::ReplacePointInConstraints(hEntity oldpt, hEntity newpt) {
  for (auto &c : SK.constraint) {
    if (c.ptA == oldpt)
      c.ptA = newpt;
    if (c.ptB == oldpt)
      c.ptB = newpt;
  }
}

//-----------------------------------------------------------------------------
// Remove constraints on hpt. Useful when removing bezier points.
//-----------------------------------------------------------------------------
void GraphicsWindow::RemoveConstraintsForPointBeingDeleted(hEntity hpt) {
  SK.constraint.ClearTags();
  for (auto &c : SK.constraint) {
    if (c.ptA == hpt || c.ptB == hpt) {
      c.tag = 1;
      (SS.deleted.constraints)++;
      if (c.type != Constraint::Type::POINTS_COINCIDENT && c.type != Constraint::Type::HORIZONTAL &&
          c.type != Constraint::Type::VERTICAL) {
        (SS.deleted.nonTrivialConstraints)++;
      }
    }
  }
  SK.constraint.RemoveTagged();
}

//-----------------------------------------------------------------------------
// Let's say that A is coincident with B, and B is coincident with C. This
// implies that A is coincident with C; but if we delete B, then both
// constraints must be deleted too (since they reference B), and A is no
// longer constrained to C. This routine adds back that constraint.
//-----------------------------------------------------------------------------
void GraphicsWindow::FixConstraintsForRequestBeingDeleted(hRequest hr) {
  Request *r = SK.GetRequest(hr);
  if (r->group != SS.GW.activeGroup)
    return;

  for (Entity &e : SK.entity) {
    if (!(e.h.isFromRequest()))
      continue;
    if (e.h.request() != hr)
      continue;

    if (e.type != Entity::Type::POINT_IN_2D && e.type != Entity::Type::POINT_IN_3D) {
      continue;
    }

    // This is a point generated by the request being deleted; so fix
    // the constraints for that.
    FixConstraintsForPointBeingDeleted(e.h);
  }
}
void GraphicsWindow::FixConstraintsForPointBeingDeleted(hEntity hpt) {
  List<hEntity> ld = {};

  SK.constraint.ClearTags();
  for (Constraint &c : SK.constraint) {
    if (c.type != Constraint::Type::POINTS_COINCIDENT)
      continue;
    if (c.group != SS.GW.activeGroup)
      continue;

    if (c.ptA == hpt) {
      ld.Add(&(c.ptB));
      c.tag = 1;
    }
    if (c.ptB == hpt) {
      ld.Add(&(c.ptA));
      c.tag = 1;
    }
  }
  // Remove constraints without waiting for regeneration; this way
  // if another point takes the place of the deleted one (e.g. when
  // removing control points of a bezier) the constraint doesn't
  // spuriously move. Similarly, subsequent calls of this function
  // (if multiple coincident points are getting deleted) will work
  // correctly.
  SK.constraint.RemoveTagged();

  // If more than one point was constrained coincident with hpt, then
  // those two points were implicitly coincident with each other. By
  // deleting hpt (and all constraints that mention it), we will delete
  // that relationship. So put it back here now.
  for (int i = 1; i < ld.n; i++) {
    Constraint::ConstrainCoincident(ld[i - 1], ld[i]);
  }
  ld.Clear();
}

//-----------------------------------------------------------------------------
// A curve by its parametric equation, helper functions for computing tangent
// arcs by a numerical method.
//-----------------------------------------------------------------------------
void GraphicsWindow::ParametricCurve::MakeFromEntity(hEntity he, bool reverse) {
  *this     = {};
  Entity *e = SK.GetEntity(he);
  if (e->type == Entity::Type::LINE_SEGMENT) {
    isLine = true;
    p0 = e->EndpointStart(), p1 = e->EndpointFinish();
    if (reverse) {
      std::swap(p0, p1);
    }
  } else if (e->type == Entity::Type::ARC_OF_CIRCLE) {
    isLine    = false;
    p0        = SK.GetEntity(e->point[0])->PointGetNum();
    Vector pe = SK.GetEntity(e->point[1])->PointGetNum();
    r         = (pe.Minus(p0)).Magnitude();
    e->ArcGetAngles(&theta0, &theta1, &dtheta);
    if (reverse) {
      std::swap(theta0, theta1);
      dtheta = -dtheta;
    }
    Entity *wrkpln = SK.GetEntity(e->workplane)->Normal();
    u              = wrkpln->NormalU();
    v              = wrkpln->NormalV();
  } else
    ssassert(false, "Unexpected entity type");
}
double GraphicsWindow::ParametricCurve::LengthForAuto() {
  if (isLine) {
    // Allow a third of the line to disappear with auto radius
    return (p1.Minus(p0)).Magnitude() / 3;
  } else {
    // But only a twentieth of the arc; shorter means fewer numerical
    // problems since the curve is more linear over shorter sections.
    return (fabs(dtheta) * r) / 20;
  }
}
Vector GraphicsWindow::ParametricCurve::PointAt(double t) {
  if (isLine) {
    return p0.Plus((p1.Minus(p0)).ScaledBy(t));
  } else {
    double theta = theta0 + dtheta * t;
    return p0.Plus(u.ScaledBy(r * cos(theta)).Plus(v.ScaledBy(r * sin(theta))));
  }
}
Vector GraphicsWindow::ParametricCurve::TangentAt(double t) {
  if (isLine) {
    return p1.Minus(p0);
  } else {
    double theta = theta0 + dtheta * t;
    Vector t     = u.ScaledBy(-r * sin(theta)).Plus(v.ScaledBy(r * cos(theta)));
    t            = t.ScaledBy(dtheta);
    return t;
  }
}
/** Changes or copies the given entity and connects it to the arc.
 * \param t Where on this parametric curve does it connect to the arc.
 * \param reuseOrig Should the original entity be modified
 * \param orig The original entity.
 * \param arc The arc that will be connected to.
 * \param arcFinish Whether to connect to the end point of the arc.
 * \param pointf When changing the original entity, whether the end point should be modified.
 */
void GraphicsWindow::ParametricCurve::CreateRequestTrimmedTo(double t, bool reuseOrig, hEntity orig,
                                                             hEntity arc, bool arcFinish,
                                                             bool pointf) {
  hRequest hr;
  Entity  *e;
  if (isLine) {
    if (reuseOrig) {
      e     = SK.GetEntity(orig);
      int i = pointf ? 1 : 0;
      SK.GetEntity(e->point[i])->PointForceTo(PointAt(t));
      ConstrainPointIfCoincident(e->point[i]);
    } else {
      hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false),
      e  = SK.GetEntity(hr.entity(0));
      SK.GetEntity(e->point[0])->PointForceTo(PointAt(t));
      SK.GetEntity(e->point[1])->PointForceTo(PointAt(1));
      ConstrainPointIfCoincident(e->point[0]);
      ConstrainPointIfCoincident(e->point[1]);
      Constraint::Constrain(Constraint::Type::PT_ON_LINE, hr.entity(1), Entity::NO_ENTITY, orig);
    }
    Constraint::Constrain(Constraint::Type::ARC_LINE_TANGENT, Entity::NO_ENTITY, Entity::NO_ENTITY,
                          arc, e->h, /*other=*/arcFinish, /*other2=*/false);
  } else {
    if (reuseOrig) {
      e     = SK.GetEntity(orig);
      int i = pointf ? 2 : 1;
      SK.GetEntity(e->point[i])->PointForceTo(PointAt(t));
      ConstrainPointIfCoincident(e->point[i]);
    } else {
      hr = SS.GW.AddRequest(Request::Type::ARC_OF_CIRCLE, /*rememberForUndo=*/false),
      e  = SK.GetEntity(hr.entity(0));
      SK.GetEntity(e->point[0])->PointForceTo(p0);
      if (dtheta > 0) {
        SK.GetEntity(e->point[1])->PointForceTo(PointAt(t));
        SK.GetEntity(e->point[2])->PointForceTo(PointAt(1));
      } else {
        SK.GetEntity(e->point[2])->PointForceTo(PointAt(t));
        SK.GetEntity(e->point[1])->PointForceTo(PointAt(1));
      }
      ConstrainPointIfCoincident(e->point[0]);
      ConstrainPointIfCoincident(e->point[1]);
      ConstrainPointIfCoincident(e->point[2]);
    }
    // The tangency constraint alone is enough to fully constrain it,
    // so there's no need for more.
    Constraint::Constrain(Constraint::Type::CURVE_CURVE_TANGENT, Entity::NO_ENTITY,
                          Entity::NO_ENTITY, arc, e->h, /*other=*/arcFinish,
                          /*other2=*/(dtheta < 0));
  }
}

//-----------------------------------------------------------------------------
// If a point in the same group as hpt, and numerically coincident with hpt,
// happens to exist, then constrain that point coincident to hpt.
//-----------------------------------------------------------------------------
void GraphicsWindow::ParametricCurve::ConstrainPointIfCoincident(hEntity hpt) {
  Entity *pt;
  pt = SK.GetEntity(hpt);
  Vector ev, ptv;
  ptv = pt->PointGetNum();

  for (Entity &e : SK.entity) {
    if (e.h == pt->h)
      continue;
    if (!e.IsPoint())
      continue;
    if (e.group != pt->group)
      continue;
    if (e.workplane != pt->workplane)
      continue;

    ev = e.PointGetNum();
    if (!ev.Equals(ptv))
      continue;

    Constraint::ConstrainCoincident(hpt, e.h);
    break;
  }
}

//-----------------------------------------------------------------------------
// A single point must be selected when this function is called. We find two
// non-construction line segments that join at this point, and create a
// tangent arc joining them.
//-----------------------------------------------------------------------------
void GraphicsWindow::MakeTangentArc() {
  if (!LockedInWorkplane()) {
    Error(_("Must be sketching in workplane to create tangent arc."));
    return;
  }

  // The point corresponding to the vertex to be rounded.
  Vector pshared = SK.GetEntity(gs.point[0])->PointGetNum();
  ClearSelection();

  // First, find two requests (that are not construction, and that are
  // in our group and workplane) that generate entities that have an
  // endpoint at our vertex to be rounded.
  int      i, c = 0;
  Entity  *ent[2];
  Request *req[2];
  hRequest hreq[2];
  hEntity  hent[2];
  bool     pointf[2];
  for (auto &r : SK.request) {
    if (r.group != activeGroup)
      continue;
    if (r.workplane != ActiveWorkplane())
      continue;
    if (r.construction)
      continue;
    if (r.type != Request::Type::LINE_SEGMENT && r.type != Request::Type::ARC_OF_CIRCLE) {
      continue;
    }

    Entity *e  = SK.GetEntity(r.h.entity(0));
    Vector  ps = e->EndpointStart(), pf = e->EndpointFinish();

    if (ps.Equals(pshared) || pf.Equals(pshared)) {
      if (c < 2) {
        // We record the entity and request and their handles,
        // and whether the vertex to be rounded is the start or
        // finish of this entity.
        ent[c]    = e;
        hent[c]   = e->h;
        req[c]    = &r;
        hreq[c]   = r.h;
        pointf[c] = (pf.Equals(pshared));
      }
      c++;
    }
  }
  if (c != 2) {
    Error(_("To create a tangent arc, select a point where two "
            "non-construction lines or circles in this group and "
            "workplane join."));
    return;
  }

  Entity *wrkpl = SK.GetEntity(ActiveWorkplane());
  Vector  wn    = wrkpl->Normal()->NormalN();

  // Based on these two entities, we make the objects that we'll use to
  // numerically find the tangent arc.
  ParametricCurve pc[2];
  pc[0].MakeFromEntity(ent[0]->h, pointf[0]);
  pc[1].MakeFromEntity(ent[1]->h, pointf[1]);

  // And thereafter we mustn't touch the entity or req ptrs,
  // because the new requests/entities we add might force a
  // realloc.
  memset(ent, 0, sizeof(ent));
  memset(req, 0, sizeof(req));

  Vector pinter;
  double r = 0.0, vv = 0.0;
  // We now do Newton iterations to find the tangent arc, and its positions
  // t back along the two curves, starting from shared point of the curves
  // at t = 0. Lots of iterations helps convergence, and this is still
  // ~10 ms for everything.
  int    iters = 1000;
  double t[2]  = {0, 0}, tp[2];
  for (i = 0; i < iters + 20; i++) {
    Vector p0 = pc[0].PointAt(t[0]), p1 = pc[1].PointAt(t[1]), t0 = pc[0].TangentAt(t[0]),
           t1 = pc[1].TangentAt(t[1]);

    VectorAtIntersectionOfLines_ret eeep =
        VectorAtIntersectionOfLines(p0, p0.Plus(t0), p1, p1.Plus(t1), false);
    pinter = eeep.intersectionPoint;

    // The sign of vv determines whether shortest distance is
    // clockwise or anti-clockwise.
    Vector v = (wn.Cross(t0)).WithMagnitude(1);
    vv       = t1.Dot(v);

    double dot   = (t0.WithMagnitude(1)).Dot(t1.WithMagnitude(1));
    double theta = acos(dot);

    if (SS.tangentArcManual) {
      r = SS.tangentArcRadius;
    } else {
      r = 200 / scale;
      // Set the radius so that no more than one third of the
      // line segment disappears.
      r = std::min(r, pc[0].LengthForAuto() * tan(theta / 2));
      r = std::min(r, pc[1].LengthForAuto() * tan(theta / 2));
      ;
    }
    // We are source-stepping the radius, to improve convergence. So
    // ramp that for most of the iterations, and then do a few at
    // the end with that constant for polishing.
    if (i < iters) {
      r *= 0.1 + 0.9 * i / ((double)iters);
    }

    // The distance from the intersection of the lines to the endpoint
    // of the arc, along each line.
    double el = r / tan(theta / 2);

    // Compute the endpoints of the arc, for each curve
    Vector pa0 = pinter.Plus(t0.WithMagnitude(el)), pa1 = pinter.Plus(t1.WithMagnitude(el));

    tp[0] = t[0];
    tp[1] = t[1];

    // And convert those points to parameter values along the curve.
    t[0] += (pa0.Minus(p0)).DivProjected(t0);
    t[1] += (pa1.Minus(p1)).DivProjected(t1);
  }

  // Stupid check for convergence, and for an out of range result (as
  // we would get, for example, if the line is too short to fit the
  // rounding arc).
  if (fabs(tp[0] - t[0]) > 1e-3 || fabs(tp[1] - t[1]) > 1e-3 || t[0] < 0.01 || t[1] < 0.01 ||
      t[0] > 0.99 || t[1] > 0.99 || IsReasonable(t[0]) || IsReasonable(t[1])) {
    Error(_("Couldn't round this corner. Try a smaller radius, or try "
            "creating the desired geometry by hand with tangency "
            "constraints."));
    return;
  }

  // Compute the location of the center of the arc
  Vector center = pc[0].PointAt(t[0]), v0inter = pinter.Minus(center);
  int    a, b;
  if (vv < 0) {
    a      = 1;
    b      = 2;
    center = center.Minus(v0inter.Cross(wn).WithMagnitude(r));
  } else {
    a      = 2;
    b      = 1;
    center = center.Plus(v0inter.Cross(wn).WithMagnitude(r));
  }

  SS.UndoRemember();

  if (SS.tangentArcModify) {
    // Delete the coincident constraint for the removed point.
    SK.constraint.ClearTags();
    for (i = 0; i < SK.constraint.n; i++) {
      Constraint *cs = &(SK.constraint[i]);
      if (cs->group != activeGroup)
        continue;
      if (cs->workplane != ActiveWorkplane())
        continue;
      if (cs->type != Constraint::Type::POINTS_COINCIDENT)
        continue;
      if (SK.GetEntity(cs->ptA)->PointGetNum().Equals(pshared)) {
        cs->tag = 1;
      }
    }
    SK.constraint.RemoveTagged();
  } else {
    // Make the original entities construction, or delete them
    // entirely, according to user preference.
    SK.GetRequest(hreq[0])->construction = true;
    SK.GetRequest(hreq[1])->construction = true;
  }

  // Create and position the new tangent arc.
  hRequest harc  = AddRequest(Request::Type::ARC_OF_CIRCLE, /*rememberForUndo=*/false);
  Entity  *earc  = SK.GetEntity(harc.entity(0));
  hEntity  hearc = earc->h;

  SK.GetEntity(earc->point[0])->PointForceTo(center);
  SK.GetEntity(earc->point[a])->PointForceTo(pc[0].PointAt(t[0]));
  SK.GetEntity(earc->point[b])->PointForceTo(pc[1].PointAt(t[1]));

  earc = NULL;

  // Modify or duplicate the original entities and connect them to the tangent arc.
  pc[0].CreateRequestTrimmedTo(t[0], SS.tangentArcModify, hent[0], hearc, /*arcFinish=*/(b == 1),
                               pointf[0]);
  pc[1].CreateRequestTrimmedTo(t[1], SS.tangentArcModify, hent[1], hearc, /*arcFinish=*/(a == 1),
                               pointf[1]);
}

hEntity GraphicsWindow::SplitLine(hEntity he, Vector pinter) {
  // Save the original endpoints, since we're about to delete this entity.
  Entity *e01  = SK.GetEntity(he);
  hEntity hep0 = e01->point[0], hep1 = e01->point[1];
  Vector  p0 = SK.GetEntity(hep0)->PointGetNum(), p1 = SK.GetEntity(hep1)->PointGetNum();

  // Add the two line segments this one gets split into.
  hRequest r0i = AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false),
           ri1 = AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
  // Don't get entities till after adding, realloc issues

  Entity *e0i = SK.GetEntity(r0i.entity(0)), *ei1 = SK.GetEntity(ri1.entity(0));

  SK.GetEntity(e0i->point[0])->PointForceTo(p0);
  SK.GetEntity(e0i->point[1])->PointForceTo(pinter);
  SK.GetEntity(ei1->point[0])->PointForceTo(pinter);
  SK.GetEntity(ei1->point[1])->PointForceTo(p1);

  ReplacePointInConstraints(hep0, e0i->point[0]);
  ReplacePointInConstraints(hep1, ei1->point[1]);
  Constraint::ConstrainCoincident(e0i->point[1], ei1->point[0]);
  return e0i->point[1];
}

hEntity GraphicsWindow::SplitCircle(hEntity he, Vector pinter) {
  Entity *circle = SK.GetEntity(he);
  if (circle->type == Entity::Type::CIRCLE) {
    // Start with an unbroken circle, split it into a 360 degree arc.
    Vector center = SK.GetEntity(circle->point[0])->PointGetNum();

    circle      = NULL; // shortly invalid!
    hRequest hr = AddRequest(Request::Type::ARC_OF_CIRCLE, /*rememberForUndo=*/false);

    Entity *arc = SK.GetEntity(hr.entity(0));

    SK.GetEntity(arc->point[0])->PointForceTo(center);
    SK.GetEntity(arc->point[1])->PointForceTo(pinter);
    SK.GetEntity(arc->point[2])->PointForceTo(pinter);

    Constraint::ConstrainCoincident(arc->point[1], arc->point[2]);
    return arc->point[1];
  } else {
    // Start with an arc, break it in to two arcs
    hEntity hc = circle->point[0], hs = circle->point[1], hf = circle->point[2];
    Vector  center = SK.GetEntity(hc)->PointGetNum(), start = SK.GetEntity(hs)->PointGetNum(),
           finish = SK.GetEntity(hf)->PointGetNum();

    circle       = NULL; // shortly invalid!
    hRequest hr0 = AddRequest(Request::Type::ARC_OF_CIRCLE, /*rememberForUndo=*/false),
             hr1 = AddRequest(Request::Type::ARC_OF_CIRCLE, /*rememberForUndo=*/false);

    Entity *arc0 = SK.GetEntity(hr0.entity(0)), *arc1 = SK.GetEntity(hr1.entity(0));

    SK.GetEntity(arc0->point[0])->PointForceTo(center);
    SK.GetEntity(arc0->point[1])->PointForceTo(start);
    SK.GetEntity(arc0->point[2])->PointForceTo(pinter);

    SK.GetEntity(arc1->point[0])->PointForceTo(center);
    SK.GetEntity(arc1->point[1])->PointForceTo(pinter);
    SK.GetEntity(arc1->point[2])->PointForceTo(finish);

    ReplacePointInConstraints(hs, arc0->point[1]);
    ReplacePointInConstraints(hf, arc1->point[2]);
    Constraint::ConstrainCoincident(arc0->point[2], arc1->point[1]);
    return arc0->point[2];
  }
}

hEntity GraphicsWindow::SplitCubic(hEntity he, Vector pinter) {
  // Save the original endpoints, since we're about to delete this entity.
  Entity     *e01 = SK.GetEntity(he);
  SBezierList sbl = {};
  e01->GenerateBezierCurves(&sbl);

  hEntity hep0 = e01->point[0], hep1 = e01->point[3 + e01->extraPoints],
          hep0n = Entity::NO_ENTITY, // the new start point
      hep1n     = Entity::NO_ENTITY, // the new finish point
      hepin     = Entity::NO_ENTITY; // the intersection point

  // The curve may consist of multiple cubic segments. So find which one
  // contains the intersection point.
  double t;
  int    i, j;
  for (i = 0; i < sbl.l.n; i++) {
    SBezier *sb = &(sbl.l[i]);
    ssassert(sb->deg == 3, "Expected a cubic bezier");

    sb->ClosestPointTo(pinter, &t, /*mustConverge=*/false);
    if (pinter.Equals(sb->PointAt(t))) {
      // Split that segment at the intersection.
      SBezier b0i, bi1, b01 = *sb;
      b01.SplitAt(t, &b0i, &bi1);

      // Add the two cubic segments this one gets split into.
      hRequest r0i = AddRequest(Request::Type::CUBIC, /*rememberForUndo=*/false),
               ri1 = AddRequest(Request::Type::CUBIC, /*rememberForUndo=*/false);
      // Don't get entities till after adding, realloc issues

      Entity *e0i = SK.GetEntity(r0i.entity(0)), *ei1 = SK.GetEntity(ri1.entity(0));

      for (j = 0; j <= 3; j++) {
        SK.GetEntity(e0i->point[j])->PointForceTo(b0i.ctrl[j]);
      }
      for (j = 0; j <= 3; j++) {
        SK.GetEntity(ei1->point[j])->PointForceTo(bi1.ctrl[j]);
      }

      Constraint::ConstrainCoincident(e0i->point[3], ei1->point[0]);
      if (i == 0)
        hep0n = e0i->point[0];
      hep1n = ei1->point[3];
      hepin = e0i->point[3];
    } else {
      hRequest r = AddRequest(Request::Type::CUBIC, /*rememberForUndo=*/false);
      Entity  *e = SK.GetEntity(r.entity(0));

      for (j = 0; j <= 3; j++) {
        SK.GetEntity(e->point[j])->PointForceTo(sb->ctrl[j]);
      }

      if (i == 0)
        hep0n = e->point[0];
      hep1n = e->point[3];
    }
  }

  sbl.Clear();

  ReplacePointInConstraints(hep0, hep0n);
  ReplacePointInConstraints(hep1, hep1n);
  return hepin;
}

hEntity GraphicsWindow::SplitEntity(hEntity he, Vector pinter) {
  Entity      *e          = SK.GetEntity(he);
  Entity::Type entityType = e->type;

  hEntity ret;
  if (e->IsCircle()) {
    ret = SplitCircle(he, pinter);
  } else if (e->type == Entity::Type::LINE_SEGMENT) {
    ret = SplitLine(he, pinter);
  } else if (e->type == Entity::Type::CUBIC || e->type == Entity::Type::CUBIC_PERIODIC) {
    ret = SplitCubic(he, pinter);
  } else {
    Error(_("Couldn't split this entity; lines, circles, or cubics only."));
    return Entity::NO_ENTITY;
  }

  // Finally, delete the request that generated the original entity.
  Request::Type reqType = EntReqTable::GetRequestForEntity(entityType);
  SK.request.ClearTags();
  for (auto &r : SK.request) {
    if (r.group != activeGroup)
      continue;
    if (r.type != reqType)
      continue;

    // If the user wants to keep the old entities around, they can just
    // mark them construction first.
    if (he == r.h.entity(0) && !r.construction) {
      r.tag = 1;
      break;
    }
  }
  DeleteTaggedRequests();

  return ret;
}

void GraphicsWindow::SplitLinesOrCurves() {
  if (!LockedInWorkplane()) {
    Error(_("Must be sketching in workplane to split."));
    return;
  }

  GroupSelection();
  int n = gs.lineSegments + gs.circlesOrArcs + gs.cubics + gs.periodicCubics;
  if (!((n == 2 && gs.points == 0) || (n == 1 && gs.points == 1))) {
    Error(_("Select two entities that intersect each other "
            "(e.g. two lines/circles/arcs or a line/circle/arc and a point)."));
    return;
  }

  bool    splitAtPoint = (gs.points == 1);
  hEntity ha = gs.entity[0], hb = splitAtPoint ? gs.point[0] : gs.entity[1];

  Entity     *ea = SK.GetEntity(ha), *eb = SK.GetEntity(hb);
  SPointList  inters = {};
  SBezierList sbla = {}, sblb = {};
  Vector      pi = Vector::From(0, 0, 0);

  SK.constraint.ClearTags();

  // First, decide the point where we're going to make the split.
  bool foundInters = false;
  if (splitAtPoint) {
    // One of the entities is a point, and this point must be on the other entity.
    // Verify that a corresponding point-coincident constraint exists for the point/entity.
    Vector p0, p1;
    if (ea->type == Entity::Type::LINE_SEGMENT) {
      p0 = ea->EndpointStart();
      p1 = ea->EndpointFinish();
    }

    for (Constraint &c : SK.constraint) {
      if (c.ptA.request() == hb.request() && c.entityA.request() == ha.request()) {
        pi = SK.GetEntity(c.ptA)->PointGetNum();

        if (ea->type == Entity::Type::LINE_SEGMENT && !pi.OnLineSegment(p0, p1)) {
          // The point isn't between line endpoints, so there isn't an actual
          // intersection.
          continue;
        }

        c.tag       = 1;
        foundInters = true;
        break;
      }
    }
  } else {
    // Compute the possibly-rational Bezier curves for each of these non-point entities...
    ea->GenerateBezierCurves(&sbla);
    eb->GenerateBezierCurves(&sblb);
    // ... and then compute the points where they intersect, based on those curves.
    sbla.AllIntersectionsWith(&sblb, &inters);

    // If there's multiple points, then take the one closest to the mouse pointer.
    if (!inters.l.IsEmpty()) {
      double  dmin = VERY_POSITIVE;
      SPoint *sp;
      for (sp = inters.l.First(); sp; sp = inters.l.NextAfter(sp)) {
        double d = ProjectPoint(sp->p).DistanceTo(currentMousePosition);
        if (d < dmin) {
          dmin = d;
          pi   = sp->p;
        }
      }
    }

    foundInters = true;
  }

  // Then, actually split the entities.
  if (foundInters) {
    SS.UndoRemember();

    // Remove any constraints we're going to replace.
    SK.constraint.RemoveTagged();

    hEntity hia = SplitEntity(ha, pi), hib = {};
    // SplitEntity adds the coincident constraints to join the split halves
    // of each original entity; and then we add the constraint to join
    // the two entities together at the split point.
    if (splitAtPoint) {
      // Remove datum point, as it has now been superseded by the split point.
      SK.request.ClearTags();
      for (Request &r : SK.request) {
        if (r.h == hb.request()) {
          if (r.type == Request::Type::DATUM_POINT) {
            // Delete datum point.
            r.tag = 1;
            FixConstraintsForRequestBeingDeleted(r.h);
          } else {
            // Add constraint if not datum point, but endpoint of line/arc etc.
            Constraint::ConstrainCoincident(hia, hb);
          }
          break;
        }
      }
      SK.request.RemoveTagged();
    } else {
      // Split second non-point entity and add constraint.
      hib = SplitEntity(hb, pi);
      if (hia.v && hib.v) {
        Constraint::ConstrainCoincident(hia, hib);
      }
    }
  } else {
    Error(_("Can't split; no intersection found."));
    return;
  }

  // All done, clean up and regenerate.
  inters.Clear();
  sbla.Clear();
  sblb.Clear();
  ClearSelection();
}
