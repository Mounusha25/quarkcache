"use client";

import React from "react";
import {
  Table, TableBody, TableCell, TableContainer, TableHead,
  TableRow, Paper, IconButton, Chip, Tooltip, Typography,
} from "@mui/material";
import DeleteIcon from "@mui/icons-material/Delete";
import { KeyListItem } from "@/types/index";

interface KeyTableProps {
  keys: KeyListItem[];
  onDelete: (key: string) => void;
}

export default function KeyTable({ keys, onDelete }: KeyTableProps) {
  if (keys.length === 0) {
    return (
      <Typography color="text.secondary" sx={{ py: 4, textAlign: "center" }}>
        No keys in the cache.
      </Typography>
    );
  }

  return (
    <TableContainer component={Paper} elevation={2}>
      <Table size="small">
        <TableHead>
          <TableRow sx={{ "& th": { fontWeight: 700 } }}>
            <TableCell>Key</TableCell>
            <TableCell>Value</TableCell>
            <TableCell>TTL</TableCell>
            <TableCell align="right">Actions</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {keys.map((row) => (
            <TableRow key={row.key} hover>
              <TableCell>
                <Typography variant="body2" sx={{ fontFamily: "monospace" }}>{row.key}</Typography>
              </TableCell>
              <TableCell>
                <Typography variant="body2"
                  sx={{ fontFamily: "monospace", maxWidth: 300, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                  {row.value}
                </Typography>
              </TableCell>
              <TableCell>
                {row.ttl_remaining !== null
                  ? <Chip label={`${row.ttl_remaining}s`} size="small" color={row.ttl_remaining < 10 ? "warning" : "default"} />
                  : <Chip label="∞" size="small" variant="outlined" />
                }
              </TableCell>
              <TableCell align="right">
                <Tooltip title={`Delete "${row.key}"`}>
                  <IconButton size="small" color="error" onClick={() => onDelete(row.key)}>
                    <DeleteIcon fontSize="small" />
                  </IconButton>
                </Tooltip>
              </TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>
  );
}
