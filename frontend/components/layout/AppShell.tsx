"use client";

import React, { useState } from "react";
import {
  AppBar, Box, CssBaseline, Drawer, IconButton,
  List, ListItem, ListItemButton, ListItemIcon, ListItemText,
  Toolbar, Typography, useTheme, useMediaQuery, Chip,
} from "@mui/material";
import MenuIcon from "@mui/icons-material/Menu";
import DashboardIcon from "@mui/icons-material/Dashboard";
import StorageIcon from "@mui/icons-material/Storage";
import BarChartIcon from "@mui/icons-material/BarChart";
import PeopleIcon from "@mui/icons-material/People";
import TerminalIcon from "@mui/icons-material/Terminal";
import SettingsIcon from "@mui/icons-material/Settings";
import AutoAwesomeIcon from "@mui/icons-material/AutoAwesome";
import SpeedIcon from "@mui/icons-material/Speed";
import FiberManualRecordIcon from "@mui/icons-material/FiberManualRecord";
import Link from "next/link";
import { usePathname } from "next/navigation";

const DRAWER_WIDTH = 230;

const NAV_ITEMS = [
  { label: "Overview",   href: "/dashboard",  icon: <DashboardIcon fontSize="small" /> },
  { label: "Explorer",   href: "/keys",        icon: <StorageIcon fontSize="small" /> },
  { label: "Metrics",    href: "/metrics",     icon: <BarChartIcon fontSize="small" /> },
  { label: "Clients",    href: "/clients",     icon: <PeopleIcon fontSize="small" /> },
  { label: "Commands",   href: "/commands",    icon: <TerminalIcon fontSize="small" /> },
  { label: "Benchmarks", href: "/benchmarks",  icon: <SpeedIcon fontSize="small" /> },
  { label: "AI Insights",href: "/ai",          icon: <AutoAwesomeIcon fontSize="small" />, highlight: true },
  { label: "Settings",   href: "/settings",    icon: <SettingsIcon fontSize="small" /> },
];

export default function AppShell({ children }: { children: React.ReactNode }) {
  const theme    = useTheme();
  const isMobile = useMediaQuery(theme.breakpoints.down("md"));
  const [open, setOpen] = useState(false);
  const pathname = usePathname();

  const drawer = (
    <Box sx={{ display: "flex", flexDirection: "column", height: "100%" }}>
      {/* Logo */}
      <Box sx={{ px: 3, py: 2.5, display: "flex", alignItems: "center", gap: 1.5 }}>
        <Box sx={{
          width: 32, height: 32, borderRadius: "50%",
          background: "linear-gradient(135deg, #7c3aed, #06b6d4)",
          display: "flex", alignItems: "center", justifyContent: "center",
        }}>
          <AutoAwesomeIcon sx={{ fontSize: 16, color: "#fff" }} />
        </Box>
        <Typography sx={{ fontWeight: 700, fontSize: "1.1rem", letterSpacing: "-0.5px" }}>
          quark cache
        </Typography>
      </Box>

      {/* Nav */}
      <Box sx={{ flexGrow: 1, overflow: "auto", px: 1 }}>
        <List dense>
          {NAV_ITEMS.map((item) => {
            const active = pathname.startsWith(item.href);
            return (
              <ListItem key={item.href} disablePadding sx={{ mb: 0.5 }}>
                <ListItemButton
                  component={Link}
                  href={item.href}
                  selected={active}
                  onClick={() => setOpen(false)}
                  sx={{
                    borderRadius: 2, px: 2, py: 1,
                    "&.Mui-selected": {
                      bgcolor: "rgba(124,58,237,0.18)",
                      "& .MuiListItemIcon-root": { color: "primary.main" },
                      "& .MuiListItemText-primary": { color: "primary.main", fontWeight: 600 },
                    },
                    "&:hover": { bgcolor: "rgba(255,255,255,0.05)" },
                  }}
                >
                  <ListItemIcon sx={{ minWidth: 34, color: active ? "primary.main" : "text.secondary" }}>
                    {item.icon}
                  </ListItemIcon>
                  <ListItemText
                    primary={item.label}
                    slotProps={{ primary: { style: { fontSize: "0.875rem" } } }}
                  />
                  {item.highlight && (
                    <Chip label="AI" size="small"
                      sx={{ height: 18, fontSize: "0.65rem", bgcolor: "primary.main", color: "#fff", ml: 1 }} />
                  )}
                </ListItemButton>
              </ListItem>
            );
          })}
        </List>
      </Box>

      {/* Version footer */}
      <Box sx={{ px: 3, py: 2 }}>
        <Typography variant="caption" color="text.disabled">QuarkCache v1.0.0</Typography>
      </Box>
    </Box>
  );

  return (
    <Box sx={{ display: "flex", minHeight: "100vh", bgcolor: "background.default" }}>
      <CssBaseline />

      {/* Top AppBar (mobile only) */}
      {isMobile && (
        <AppBar position="fixed" elevation={0}
          sx={{ bgcolor: "background.paper", borderBottom: "1px solid", borderColor: "divider" }}>
          <Toolbar>
            <IconButton color="inherit" edge="start" onClick={() => setOpen(!open)} sx={{ mr: 2 }}>
              <MenuIcon />
            </IconButton>
            <Typography sx={{ fontWeight: 700 }}>quark cache</Typography>
            <Box sx={{ flexGrow: 1 }} />
            <Chip icon={<FiberManualRecordIcon sx={{ fontSize: "10px !important", color: "#22c55e !important" }} />}
              label="Live" size="small"
              sx={{ bgcolor: "rgba(34,197,94,0.12)", color: "#22c55e", border: "1px solid rgba(34,197,94,0.3)" }} />
          </Toolbar>
        </AppBar>
      )}

      {/* Sidebar Drawer */}
      <Drawer
        variant={isMobile ? "temporary" : "permanent"}
        open={isMobile ? open : true}
        onClose={() => setOpen(false)}
        sx={{
          width: DRAWER_WIDTH, flexShrink: 0,
          "& .MuiDrawer-paper": {
            width: DRAWER_WIDTH, boxSizing: "border-box",
            bgcolor: "background.paper",
            borderRight: "1px solid rgba(255,255,255,0.06)",
          },
        }}
      >
        {drawer}
      </Drawer>

      {/* Main content */}
      <Box component="main" sx={{ flexGrow: 1, minHeight: "100vh", display: "flex", flexDirection: "column" }}>
        {/* Top bar */}
        <Box sx={{
          display: "flex", alignItems: "center", justifyContent: "flex-end",
          px: 3, py: 1.5, borderBottom: "1px solid rgba(255,255,255,0.06)",
          bgcolor: "background.paper",
        }}>
          {isMobile && <Box sx={{ height: 64 }} />}
          <Chip
            icon={<FiberManualRecordIcon sx={{ fontSize: "10px !important", color: "#22c55e !important" }} />}
            label="Live"
            size="small"
            sx={{ bgcolor: "rgba(34,197,94,0.12)", color: "#22c55e", border: "1px solid rgba(34,197,94,0.3)" }}
          />
        </Box>

        <Box sx={{ flexGrow: 1, p: 3, overflow: "auto" }}>
          {children}
        </Box>
      </Box>
    </Box>
  );
}
