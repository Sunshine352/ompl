/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#include "ompl/extension/samplingbased/kinematic/SpaceInformationKinematic.h"
#include <math_utils/angles.h>
#include <algorithm>
#include <queue>

double ompl::SpaceInformationKinematic::StateKinematicL2SquareDistanceEvaluator::operator()(const State_t s1, const State_t s2)
{
    const StateKinematic_t sk1 = static_cast<const StateKinematic_t>(s1);
    const StateKinematic_t sk2 = static_cast<const StateKinematic_t>(s2);
    const unsigned int     dim = m_si->getStateDimension();
    
    double dist = 0.0;
    for (unsigned int i = 0 ; i < dim ; ++i)
    {	 
	double diff = m_si->getStateComponent(i).type == StateComponent::ANGLE ? 
	    math_utils::shortest_angular_distance(sk1->values[i], sk2->values[i]) : sk1->values[i] - sk2->values[i];
	dist += diff * diff;
    }
    return dist;
}

void ompl::SpaceInformationKinematic::printState(const StateKinematic_t state, FILE* out) const
{
    for (unsigned int i = 0 ; i < m_stateDimension ; ++i)
	fprintf(out, "%0.6f ", state->values[i]);
    fprintf(out, "\n");
}
void ompl::SpaceInformationKinematic::sample(StateKinematic_t state)
{
    for (unsigned int i = 0 ; i < m_stateDimension ; ++i)
	if (m_stateComponent[i].type == StateComponent::QUATERNION)
	{
	    fprintf(stderr, "Sampling quaternions has not been implemented yet!\n");	    
	    i += 3;
	}
	else
	state->values[i] = random_utils::uniform(&m_rngState, m_stateComponent[i].minValue, m_stateComponent[i].maxValue);	    
}

void ompl::SpaceInformationKinematic::sampleNear(StateKinematic_t state, const StateKinematic_t near, double rho)
{
    for (unsigned int i = 0 ; i < m_stateDimension ; ++i)
	if (m_stateComponent[i].type == StateComponent::QUATERNION)
	{
	    fprintf(stderr, "Sampling quaternions has not been implemented yet!\n");	    
	    i += 3;
	}
	else
	    state->values[i] = m_stateComponent[i].type == StateComponent::FIXED ?
		near->values[i] :
		random_utils::uniform(&m_rngState, 
				      std::max(m_stateComponent[i].minValue, near->values[i] - rho), 
				      std::min(m_stateComponent[i].maxValue, near->values[i] + rho));
}

bool ompl::SpaceInformationKinematic::checkMotion(const StateKinematic_t s1, const StateKinematic_t s2)
{
    /* assume motion starts in a valid configuration so s1 is valid */
    if (!isValid(s2))
	return false;

    /* find out how many divisions we need */
    int nd = 1;
    for (unsigned int i = 0 ; i < m_stateDimension ; ++i)
    {
	int d = 1 + (int)(fabs(s1->values[i] - s2->values[i]) / m_stateComponent[i].resolution);
	if (nd < d)
	    nd = d;
    }

    /* find out the step size as a vector */
    double step[m_stateDimension];    
    for (unsigned int i = 0 ; i < m_stateDimension ; ++i)
    	step[i] = (s2->values[i] - s1->values[i]) / (double)nd;
    
    /* initialize the queue of test positions */
    std::queue< std::pair<int, int> > pos;
    if (nd > 2)
	pos.push(std::make_pair(1, nd - 1));
    
    /* temporary storage for the checked state */
    StateKinematic test(m_stateDimension);

    /* repeatedly subdivide the path segment in the middle (and check the middle) */
    while (!pos.empty())
    {
	std::pair<int, int> x = pos.front();

	int mid = (x.first + x.second) / 2;

	for (unsigned int j = 0 ; j < m_stateDimension ; ++j)
	    test.values[j] = s1->values[j] + (double)mid * step[j];

	if (!isValid(&test))
	    return false;

	pos.pop();
	
	if (x.first < mid)
	    pos.push(std::make_pair(x.first, mid - 1));
	if (x.second > mid)
	    pos.push(std::make_pair(mid + 1, x.second));
    }
            
    return true;
}
    
void ompl::SpaceInformationKinematic::smoothVertices(PathKinematic_t path)
{
    if (path->states.size() < 3)
	return;

    unsigned int nochange = 0;
    
    for (unsigned int i = 0 ; i < m_smoother.maxSteps && nochange < m_smoother.maxEmptySteps ; ++i, ++nochange)
    {
	int count = path->states.size();
	int maxN  = count - 1;
	int range = 1 + (int)((double)count * m_smoother.rangeRatio);
	
	int p1 = random_utils::uniformInt(&m_rngState, 0, maxN);
	int p2 = random_utils::uniformInt(&m_rngState, std::max(p1 - range, 0), std::min(maxN, p1 + range));
	if (abs(p1 - p2) < 2)
	{
	    if (p1 < maxN - 1)
		p2 = p1 + 2;
	    else
		if (p1 > 1)
		    p2 = p1 - 2;
		else
		    continue;
	}

	if (p1 > p2)
	    std::swap(p1, p2);
	
	if (checkMotion(path->states[p1], path->states[p2]))
	{
	    for (int i = p1 + 1 ; i < p2 ; ++i)
		delete path->states[i];
	    path->states.erase(path->states.begin() + p1 + 1, path->states.begin() + p2);
	    nochange = 0;
	}
    }
}

void ompl::SpaceInformationKinematic::printSettings(FILE *out) const
{
    fprintf(out, "Kinematic state space settings:\n");
    fprintf(out, "  - dimension = %u\n", m_stateDimension);
    fprintf(out, "  - start states:\n");
    for (unsigned int i = 0 ; i < getStartStateCount() ; ++i)
	 printState(dynamic_cast<const StateKinematic_t>(getStartState(i)), out);
    fprintf(out, "  - goal = %p\n", m_goal);
    fprintf(out, "  - bounding box:\n");
    for (unsigned int i = 0 ; i < m_stateDimension ; ++i)
	fprintf(out, "[%f, %f](%f) ", m_stateComponent[i].minValue, m_stateComponent[i].maxValue, m_stateComponent[i].resolution);
    fprintf(out, "\n");
}
