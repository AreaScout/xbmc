/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GBMUtils.h"

#include "utils/log.h"

#include <mutex>

using namespace KODI::WINDOWING::GBM;

namespace
{
std::once_flag flag;
}

bool CGBMUtils::CreateDevice(int fd)
{
  auto device = gbm_create_device(fd);
  if (!device)
  {
    CLog::Log(LOGERROR, "CGBMUtils::{} - failed to create device: {}", __FUNCTION__,
              strerror(errno));
    return false;
  }

  m_device.reset(new CGBMDevice(device));

  return true;
}

CGBMUtils::CGBMDevice::CGBMDevice(gbm_device* device) : m_device(device)
{
}

bool CGBMUtils::CGBMDevice::CreateSurface(
    int width, int height, uint32_t format, const uint64_t* modifiers, const int modifiers_count)
{
  gbm_surface* surface{nullptr};
#if defined(HAS_GBM_MODIFIERS)
  if (modifiers)
  {
    surface = gbm_surface_create_with_modifiers(m_device, width, height, format, modifiers,
                                                modifiers_count);
  }
#endif
  if (!surface)
  {
    surface = gbm_surface_create(m_device, width, height, format,
                                 GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  }

  if (!surface)
  {
    CLog::Log(LOGERROR, "CGBMUtils::{} - failed to create surface: {}", __FUNCTION__,
              strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "CGBMUtils::{} - created surface with size {}x{}", __FUNCTION__, width,
            height);

  m_surface.reset(new CGBMSurface(surface));

  return true;
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurface(gbm_surface* surface) : m_surface(surface)
{
}

#define MAX_SURFACE_BUFFERS 3
CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer* CGBMUtils::CGBMDevice::CGBMSurface::
    LockFrontBuffer()
{
 /* Fix for ODROID XU4, gbm_surface_has_free_buffers doesn't seem to report if there
  * are no buffers available instead GEM buffers are running out, so we manually empty
  * the buffers here for a maximum of three
  */
  std::call_once(
     flag, [this]() { CLog::Log(LOGDEBUG, "CGBMUtils - using {} buffers", MAX_SURFACE_BUFFERS); });

  if (m_buffers.size() >= MAX_SURFACE_BUFFERS)
  {
     while (!m_buffers.empty())
     {
       m_buffers.front();
       m_buffers.pop();
     }
  }

  m_buffers.emplace(std::make_unique<CGBMSurfaceBuffer>(m_surface));

  return m_buffers.back().get();
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::CGBMSurfaceBuffer(gbm_surface* surface)
  : m_surface(surface), m_buffer(gbm_surface_lock_front_buffer(surface))
{
}

CGBMUtils::CGBMDevice::CGBMSurface::CGBMSurfaceBuffer::~CGBMSurfaceBuffer()
{
  if (m_surface && m_buffer)
    gbm_surface_release_buffer(m_surface, m_buffer);
}
